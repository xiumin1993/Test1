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
    // 初始化 pa_ringbuffer：元素大小 = 每帧字节数 (2 bytes * 2 channels)
    // 元素个数 = 帧数，数据区 = ringBufferData_
    ring_buffer_size_t result = PaUtil_InitializeRingBuffer(
            &ringBuffer_,
            sizeof(int16_t) * 2,
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
            ->setSharingMode(oboe::SharingMode::Exclusive)
            ->setFormat(oboe::AudioFormat::I16)
            ->setChannelCount(2)
            ->setSampleRate(48000)
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

ring_buffer_size_t AudioEngine::writeAudioData(const int16_t* data, int32_t numFrames) {
    if (!data || numFrames <= 0) return 0;

    ring_buffer_size_t written = PaUtil_WriteRingBuffer(
            &ringBuffer_, data, numFrames);

    if (written < numFrames) {
        LOGI("Ring buffer overflow: requested %d, wrote %d", numFrames, written);
    }
    return written;
}

oboe::DataCallbackResult AudioEngine::onAudioReady(
        oboe::AudioStream *oboeStream,
        void *audioData,
        int32_t numFrames) {

    // 从环形缓冲区读取数据
    ring_buffer_size_t framesRead = PaUtil_ReadRingBuffer(
            &ringBuffer_, audioData, numFrames);

    // 如果读取不足，用静音填充剩余部分
    if (framesRead < numFrames) {
        int32_t framesToFill = numFrames - framesRead;
        auto *out = static_cast<int16_t *>(audioData);
        memset(out + framesRead * 2, 0, framesToFill * 2 * sizeof(int16_t));
    }

    return oboe::DataCallbackResult::Continue;
}