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
    ring_buffer_size_t result = PaUtil_InitializeRingBuffer(
            &ringBuffer_,
            sizeof(int16_t) * kChannelCount,   // 每帧 4 字节（2 字节 * 2 声道）
            kRingBufferFrames,
            ringBufferData_
    );
    if (result < 0) {
        LOGI("Failed to initialize ring buffer");
        return false;
    }
    LOGI("Ring buffer initialized: %d frames", kRingBufferFrames);
    return true;
}

bool AudioEngine::start() {
    oboe::AudioStreamBuilder builder;
    oboe::Result result = builder.setDirection(oboe::Direction::Output)
            ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
            ->setSharingMode(oboe::SharingMode::Shared)   // ← 从 Exclusive 改成 Shared
            ->setFormat(oboe::AudioFormat::I16)
            ->setChannelCount(kChannelCount)
            ->setSampleRate(kSampleRate)
            ->setDataCallback(this)
            ->openStream(stream_);

    if (result != oboe::Result::OK) {
        LOGI("Failed to open stream: %s", oboe::convertToText(result));
        return false;
    }

    result = stream_->requestStart();
    if (result != oboe::Result::OK) {
        LOGI("Failed to start stream: %s", oboe::convertToText(result));
        return false;
    }

    LOGI("Oboe stream started");
    return true;
}

void AudioEngine::stop() {
    if (stream_) {
        stream_->stop();
        stream_->close();
        stream_.reset();
        LOGI("Oboe stream stopped");
    }
}

// ==================== 关键：setVolume 实现 ====================
void AudioEngine::setVolume(float volume) {
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    volume_.store(volume, std::memory_order_relaxed);
}
// =============================================================

ring_buffer_size_t AudioEngine::writeAudioData(const int16_t* data, int32_t numFrames) {
    if (!data || numFrames <= 0) return 0;
    return PaUtil_WriteRingBuffer(&ringBuffer_, data, numFrames);
}

oboe::DataCallbackResult AudioEngine::onAudioReady(
        oboe::AudioStream *oboeStream,
        void *audioData,
        int32_t numFrames) {

    // 从环形缓冲区读取数据
    ring_buffer_size_t framesRead = PaUtil_ReadRingBuffer(
            &ringBuffer_, audioData, numFrames);

    // 数据不足时用静音填充，避免爆音
    if (framesRead < numFrames) {
        auto framesToFill = static_cast<int32_t>(numFrames - framesRead);
        auto *out = static_cast<int16_t *>(audioData);
        memset(out + framesRead * kChannelCount, 0,
               framesToFill * kChannelCount * sizeof(int16_t));
    }

    // 应用音量（只做 atomic read + 乘法，无 JNI、无锁、无分配）
    float vol = volume_.load(std::memory_order_relaxed);
    if (vol < 0.999f) {
        auto *samples = static_cast<int16_t *>(audioData);
        int32_t totalSamples = numFrames * kChannelCount;
        for (int32_t i = 0; i < totalSamples; ++i) {
            samples[i] = static_cast<int16_t>(static_cast<float>(samples[i]) * vol);
        }
    }

    return oboe::DataCallbackResult::Continue;
}