#include "roc_receiver.h"
#include "audio_engine.h"
#include <android/log.h>
#include <chrono>
#include <cstring>

// Roc Toolkit 头文件
extern "C" {
#include "roc/context.h"
#include "roc/receiver.h"
#include "roc/endpoint.h"
#include "roc/config.h"
#include "roc/frame.h"
}

#define LOG_TAG "RocReceiver"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

RocReceiver::RocReceiver() = default;

RocReceiver::~RocReceiver() {
    stop();
}

bool RocReceiver::start(AudioEngine& engine, int port) {
    if (running_.load()) {
        LOGI("RocReceiver already running");
        return true;
    }

    engine_ = &engine;
    port_ = port;

    // 1. 打开 Roc context
    roc_context_config context_config;
    memset(&context_config, 0, sizeof(context_config));
    if (roc_context_open(&context_config, &context_) < 0) {
        LOGE("Failed to open roc context");
        return false;
    }

    // 2. 配置 receiver：48kHz / stereo / PCM_S16
    roc_receiver_config receiver_config;
    memset(&receiver_config, 0, sizeof(receiver_config));
    receiver_config.frame_encoding.rate = 48000;
    receiver_config.frame_encoding.format = ROC_FORMAT_PCM_FLOAT32;
    receiver_config.frame_encoding.channels = ROC_CHANNEL_LAYOUT_STEREO;
    // EXTERNAL：read 为非阻塞，节奏由外部时钟提供。
    // 输出目标是 Oboe（声卡），照 roc/config.h 的说明必须用 EXTERNAL，
    // 否则 CPU 定时器与音频设备时钟的偏差会逐渐累积成欠载或溢出。
    // 代价是收包线程必须自己做节流，见 receiveThreadFunc()。
    receiver_config.clock_source = ROC_CLOCK_SOURCE_EXTERNAL;
    receiver_config.target_latency = 50000000;    // 50ms

    if (roc_receiver_open(context_, &receiver_config, &receiver_) < 0) {
        LOGE("Failed to open roc receiver");
        roc_context_close(context_);
        context_ = nullptr;
        return false;
    }

    // 3. 创建 endpoint 并绑定
    roc_endpoint* source_endpoint = nullptr;
    if (roc_endpoint_allocate(&source_endpoint) < 0) {
        LOGE("Failed to allocate source endpoint");
        roc_receiver_close(receiver_);
        roc_context_close(context_);
        receiver_ = nullptr;
        context_ = nullptr;
        return false;
    }
    roc_endpoint_set_protocol(source_endpoint, ROC_PROTO_RTP_RS8M_SOURCE);
    roc_endpoint_set_host(source_endpoint, "0.0.0.0");
    roc_endpoint_set_port(source_endpoint, port_);

    if (roc_receiver_bind(receiver_, ROC_SLOT_DEFAULT,
                          ROC_INTERFACE_AUDIO_SOURCE, source_endpoint) < 0) {
        LOGE("Failed to bind source endpoint");
        roc_endpoint_deallocate(source_endpoint);
        roc_receiver_close(receiver_);
        roc_context_close(context_);
        receiver_ = nullptr;
        context_ = nullptr;
        return false;
    }
    roc_endpoint_deallocate(source_endpoint);

    // 4. 绑定 repair endpoint（FEC）
    roc_endpoint* repair_endpoint = nullptr;
    if (roc_endpoint_allocate(&repair_endpoint) < 0) {
        LOGE("Failed to allocate repair endpoint");
        roc_receiver_close(receiver_);
        roc_context_close(context_);
        receiver_ = nullptr;
        context_ = nullptr;
        return false;
    }
    roc_endpoint_set_protocol(repair_endpoint, ROC_PROTO_RS8M_REPAIR);
    roc_endpoint_set_host(repair_endpoint, "0.0.0.0");
    roc_endpoint_set_port(repair_endpoint, port_ + 1);

    if (roc_receiver_bind(receiver_, ROC_SLOT_DEFAULT,
                          ROC_INTERFACE_AUDIO_REPAIR, repair_endpoint) < 0) {
        LOGE("Failed to bind repair endpoint");
        roc_endpoint_deallocate(repair_endpoint);
        roc_receiver_close(receiver_);
        roc_context_close(context_);
        receiver_ = nullptr;
        context_ = nullptr;
        return false;
    }
    roc_endpoint_deallocate(repair_endpoint);

    // 5. 启动接收线程
    running_.store(true);
    thread_ = std::thread(&RocReceiver::receiveThreadFunc, this);

    LOGI("Roc receiver started on port %d", port_);
    return true;
}

void RocReceiver::stop() {
    if (!running_.load()) return;
    running_.store(false);
    if (thread_.joinable()) thread_.join();

    if (receiver_) {
        roc_receiver_close(receiver_);
        receiver_ = nullptr;
    }
    if (context_) {
        roc_context_close(context_);
        context_ = nullptr;
    }
    LOGI("Roc receiver stopped");
}

void RocReceiver::receiveThreadFunc() {
    constexpr int kSampleRate = 48000;                    // 与 AudioEngine 保持一致
    constexpr int kFramesPerRead = 480;                   // 10ms @ 48kHz
    constexpr int kSamplesPerRead = kFramesPerRead * 2;   // 立体声交织

    // 本地缓冲目标水位。网络抖动已由 Roc 内部按 target_latency(50ms) 吸收，
    // 这里只需吸收本线程的调度抖动，给 2 个读取块 = 960 帧 = 20ms。
    // 若实机出现欠载断音，可上调到 2400（50ms）；调小则延迟更低但更怕卡顿。
    constexpr int kTargetLevelFrames = kFramesPerRead * 2;

    // 单次最长休眠，保证 stop() 后能及时退出，同时避免忙循环
    constexpr int kMaxWaitMs = 20;

    // read 失败通常不可自愈，连续失败到这个次数就放弃，避免日志刷屏 + 空转
    constexpr int kMaxConsecutiveErrors = 50;

    if (!engine_) {
        LOGE("RocReceiver: engine is null");
        return;
    }

    // Roc 输出 float32，直接使用，无需转换
    float pcmBuffer[kSamplesPerRead];

    roc_frame frame;
    memset(&frame, 0, sizeof(frame));
    frame.samples = pcmBuffer;
    // samples_size 是「缓冲区字节数」，不是样本数也不是帧数
    frame.samples_size = sizeof(pcmBuffer);

    int consecutiveErrors = 0;

    while (running_.load()) {
        // ==================== 水位节流 ====================
        // EXTERNAL 时钟下 roc_receiver_read 不阻塞，若不加控制会把 341ms
        // 的环形缓冲瞬间灌满，反而白白增加延迟。这里让缓冲维持在目标水位：
        // 播放侧的消耗速度（声卡时钟）反过来决定了这里的生产速度。
        const int32_t level = static_cast<int32_t>(engine_->getBufferLevelFrames());
        if (level + kFramesPerRead > kTargetLevelFrames) {
            int32_t excessFrames = level + kFramesPerRead - kTargetLevelFrames;
            int waitMs = excessFrames * 1000 / kSampleRate;
            if (waitMs < 1) waitMs = 1;
            if (waitMs > kMaxWaitMs) waitMs = kMaxWaitMs;
            std::this_thread::sleep_for(std::chrono::milliseconds(waitMs));
            continue;
        }

        int result = roc_receiver_read(receiver_, &frame);
        if (result < 0) {
            LOGE("roc_receiver_read failed: %d", result);
            // 原实现此处直接 continue，会变成 100% CPU 忙循环并刷日志，
            // 且缓冲永远得不到数据。这里退避后重试，超阈值则退出线程。
            if (++consecutiveErrors >= kMaxConsecutiveErrors) {
                LOGE("too many consecutive read errors, abort receive thread");
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        consecutiveErrors = 0;

        // 直接将 float32 PCM 写入 AudioEngine 的环形缓冲区
        ring_buffer_size_t written =
                engine_->writeAudioData(pcmBuffer, kFramesPerRead);
        if (written < kFramesPerRead) {
            LOGI("Ring buffer overflow: wrote %ld/%d",
                 (long)written, kFramesPerRead);
        }
    }
}