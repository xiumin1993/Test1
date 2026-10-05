#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include <oboe/Oboe.h>
#include <memory>
#include <cstdint>
#include <atomic>
#include "pa_ringbuffer.h"

class AudioEngine : public oboe::AudioStreamDataCallback {
public:
    AudioEngine();
    ~AudioEngine() override;

    bool start();
    void stop();

    /**
     * 写入 float32 PCM 数据到环形缓冲区
     * @param data      float32 交错立体声 PCM 数据，取值范围 [-1.0, 1.0]
     * @param numFrames 帧数（每帧包含左右两个 float）
     * @return          实际写入的帧数
     */
    ring_buffer_size_t writeAudioData(const float* data, int32_t numFrames);

    void setVolume(float volume);

    oboe::DataCallbackResult onAudioReady(
            oboe::AudioStream *oboeStream,
            void *audioData,
            int32_t numFrames) override;

private:
    static constexpr int kSampleRate = 48000;
    static constexpr int kChannelCount = 2;
    static constexpr int kRingBufferFrames = 4096;   // ~85ms @48kHz

    std::shared_ptr<oboe::AudioStream> stream_;
    PaUtilRingBuffer ringBuffer_{};
    float ringBufferData_[kRingBufferFrames * kChannelCount]{};   // float32

    std::atomic<float> volume_{0.0f};

    bool initRingBuffer();
};

#endif // AUDIO_ENGINE_H