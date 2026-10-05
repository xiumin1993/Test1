#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include <oboe/Oboe.h>
#include <memory>
#include <cstdint>
#include <atomic>
#include "../ringbuffer/pa_ringbuffer.h"

class AudioEngine : public oboe::AudioStreamDataCallback {
public:
    AudioEngine();
    ~AudioEngine() override;

    /**
     * 启动 Oboe 播放引擎
     * @return true 表示启动成功
     */
    bool start();

    /**
     * 停止 Oboe 播放引擎
     */
    void stop();

    /**
     * 写入 PCM 音频数据到环形缓冲区
     * @param data      16-bit 交错立体声 PCM 数据
     * @param numFrames 帧数（每帧包含左右两个 short）
     * @return          实际写入的帧数
     */
    ring_buffer_size_t writeAudioData(const int16_t* data, int32_t numFrames);

    /**
     * 设置音量系数（线程安全，可在任意线程调用）
     * @param volume 音量系数，范围 [0.0, 1.0]，超出范围会自动限幅
     */
    void setVolume(float volume);

    /**
     * Oboe 音频数据回调（由音频线程调用）
     */
    oboe::DataCallbackResult onAudioReady(
            oboe::AudioStream *oboeStream,
            void *audioData,
            int32_t numFrames) override;

private:
    static constexpr int kSampleRate = 48000;       // 采样率 48kHz
    static constexpr int kChannelCount = 2;         // 立体声
    static constexpr int kRingBufferFrames = 4096;  // 环形缓冲区帧数（约 85ms @48kHz）

    std::shared_ptr<oboe::AudioStream> stream_;
    PaUtilRingBuffer ringBuffer_{};
    int16_t ringBufferData_[kRingBufferFrames * kChannelCount]{};

    // 音量系数，由 UI 线程写入，音频线程读取
    std::atomic<float> volume_{0.0f};

    /**
     * 初始化环形缓冲区
     * @return true 表示初始化成功
     */
    bool initRingBuffer();
};

#endif // AUDIO_ENGINE_H