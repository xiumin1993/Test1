#include "roc_receiver.h"
#include "audio_engine.h"
#include <android/log.h>
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
    receiver_config.clock_source = ROC_CLOCK_SOURCE_INTERNAL;
    receiver_config.target_latency = 50000000; // 50ms 目标延迟

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
    constexpr int kFramesPerRead = 480;                   // 10ms @ 48kHz
    constexpr int kSamplesPerRead = kFramesPerRead * 2;   // 立体声交织

    // Roc 输出 float32，直接使用，无需转换
    float pcmBuffer[kSamplesPerRead];

    roc_frame frame;
    memset(&frame, 0, sizeof(frame));
    frame.samples = pcmBuffer;
    // samples_size 是「缓冲区字节数」，不是样本数也不是帧数
    frame.samples_size = sizeof(pcmBuffer);

    while (running_.load()) {
        int result = roc_receiver_read(receiver_, &frame);
        if (result < 0) {
            LOGE("roc_receiver_read failed: %d", result);
            continue;
        }

        // 直接将 float32 PCM 写入 AudioEngine 的环形缓冲区
        if (engine_) {
            ring_buffer_size_t written =
                    engine_->writeAudioData(pcmBuffer, kFramesPerRead);
            if (written < kFramesPerRead) {
                LOGI("Ring buffer overflow: wrote %ld/%d",
                     (long)written, kFramesPerRead);
            }
        }
    }
}