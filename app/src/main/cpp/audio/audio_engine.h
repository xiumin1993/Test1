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
    * 返回当前环形缓冲区里堆积的音频时长（毫秒）
    * 反映抖动缓冲的占用情况
    */
    float getBufferLevelMs() const;

    /**
     * 返回当前环形缓冲区里堆积的帧数（供生产者做水位控制）
     */
    ring_buffer_size_t getBufferLevelFrames() const;

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
    // 环形缓冲容量（单位=帧，非字节）：
    // 16384 帧 ÷ 48000 帧/秒 × 1000 = 341.3 ms
    // 必须是 2 的幂，否则 PaUtil_InitializeRingBuffer 返回 -1
    // 实际占用 = 可用帧数，不会到满；341ms 只是溢出上限
    static constexpr int kRingBufferFrames = 16384;

    std::shared_ptr<oboe::AudioStream> stream_;
    PaUtilRingBuffer ringBuffer_{};
    float ringBufferData_[kRingBufferFrames * kChannelCount]{};   // float32

    std::atomic<float> volume_{0.0f};

    bool initRingBuffer();
};

#endif // AUDIO_ENGINE_H