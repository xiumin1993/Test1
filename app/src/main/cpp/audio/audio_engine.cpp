#include "audio_engine.h"
#include <android/log.h>
#include <cstring>

#define LOG_TAG "AudioEngine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

AudioEngine::AudioEngine() {
    initRingBuffer();
}

AudioEngine::~AudioEngine() {
    stop();
}

bool AudioEngine::initRingBuffer() {
    // 每帧 8 字节（2 声道 × 4 字节 float）
    ring_buffer_size_t result = PaUtil_InitializeRingBuffer(
            &ringBuffer_,
            sizeof(float) * kChannelCount,
            kRingBufferFrames,
            ringBufferData_
    );
    if (result < 0) {
        LOGI("Failed to initialize ring buffer");
        return false;
    }
    LOGI("Ring buffer initialized: %d frames (%.0f ms)",
         kRingBufferFrames, kRingBufferFrames * 1000.0f / kSampleRate);
    return true;
}

bool AudioEngine::start() {
    // 防重入：已启动就忽略
    if (stream_) {
        LOGI("AudioEngine already started, ignoring");
        return true;
    }

    // ==================== Oboe 官方推荐配置 ====================
    oboe::AudioStreamBuilder builder;

    // 1. 请求低延迟模式（官方文档：必须设置）
    builder.setPerformanceMode(oboe::PerformanceMode::LowLatency);

    // 2. 请求独占模式（官方文档：获得最低延迟）
    builder.setSharingMode(oboe::SharingMode::Exclusive);

    // 3. 音频格式：Float32，立体声
    builder.setFormat(oboe::AudioFormat::Float);
    builder.setChannelCount(kChannelCount);

    // 4. 采样率：使用设备原生采样率（48000Hz），避免重采样
    builder.setSampleRate(kSampleRate);

    // 5. 必须使用回调（官方文档推荐）
    builder.setDataCallback(this);

    // 打开流
    oboe::Result result = builder.openStream(stream_);

    if (result != oboe::Result::OK) {
        LOGI("Failed to open stream: %s", oboe::convertToText(result));
        stream_.reset();
        return false;
    }

    // ==================== 缓冲区大小：官方推荐 2 个突发周期 ====================
    // 官方文档：设置缓冲区为 getFramesPerBurst() * 2
    int32_t framesPerBurst = stream_->getFramesPerBurst();
    int32_t targetBufferSize = framesPerBurst * 2;
    stream_->setBufferSizeInFrames(targetBufferSize);

    LOGI("Oboe buffer: framesPerBurst=%d, actualBufferSize=%d (%.1f ms)",
         framesPerBurst,
         stream_->getBufferSizeInFrames(),
         stream_->getBufferSizeInFrames() * 1000.0f / kSampleRate);

    // 启动流
    result = stream_->requestStart();
    if (result != oboe::Result::OK) {
        LOGI("Failed to start stream: %s", oboe::convertToText(result));
        stream_->close();
        stream_.reset();
        return false;
    }

    LOGI("Oboe stream started (LowLatency + Exclusive, float32)");
    return true;
}

ring_buffer_size_t AudioEngine::getBufferLevelFrames() const {
    // PaUtil_GetRingBufferReadAvailable 返回"当前可读的帧数"
    // 也就是生产者写入但消费者还没读取的帧数
    // 注意：该值不是原子的，仅用于水位判断和 UI 显示，不用于同步
    return PaUtil_GetRingBufferReadAvailable(
            const_cast<PaUtilRingBuffer*>(&ringBuffer_));
}

float AudioEngine::getBufferLevelMs() const {
    return static_cast<float>(getBufferLevelFrames()) * 1000.0f / kSampleRate;
}

void AudioEngine::stop() {
    if (stream_) {
        stream_->stop();
        stream_->close();
        stream_.reset();
        LOGI("Oboe stream stopped");
    }
}

void AudioEngine::setVolume(float volume) {
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    volume_.store(volume, std::memory_order_relaxed);
}

ring_buffer_size_t AudioEngine::writeAudioData(const float* data, int32_t numFrames) {
    if (!data || numFrames <= 0) return 0;
    return PaUtil_WriteRingBuffer(&ringBuffer_, data, numFrames);
}

oboe::DataCallbackResult AudioEngine::onAudioReady(
        oboe::AudioStream *oboeStream,
        void *audioData,
        int32_t numFrames) {

    // ==================== 回调内禁止阻塞操作 ====================
    // 官方文档：避免内存分配、文件 I/O、锁等待、Sleep、繁重计算
    // 本回调仅做：环形缓冲区读取 + 静音填充 + 音量缩放

    float *out = static_cast<float *>(audioData);
    int32_t totalSamples = numFrames * kChannelCount;

    // 从环形缓冲区读取数据（无锁操作）
    ring_buffer_size_t framesRead = PaUtil_ReadRingBuffer(
            &ringBuffer_, audioData, numFrames);

    // 数据不足时用静音填充（避免爆音）
    if (framesRead < numFrames) {
        int32_t offset = static_cast<int32_t>(framesRead) * kChannelCount;
        memset(out + offset, 0,
               (totalSamples - offset) * sizeof(float));
    }

    // 应用音量（纯数学运算，无阻塞）
    float vol = volume_.load(std::memory_order_relaxed);
    if (vol < 0.999f) {
        for (int32_t i = 0; i < totalSamples; ++i) {
            out[i] *= vol;
        }
    }

    return oboe::DataCallbackResult::Continue;
}