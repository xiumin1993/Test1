#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include <oboe/Oboe.h>
#include <memory>
#include <cstdint>
#include "pa_ringbuffer.h"

class AudioEngine : public oboe::AudioStreamDataCallback {
public:
    AudioEngine();
    ~AudioEngine() override;

    bool start();
    void stop();

    // 供 Roc 接收线程调用，写入 PCM 数据
    // 返回实际写入的帧数
    ring_buffer_size_t writeAudioData(const int16_t* data, int32_t numFrames);

    // Oboe 回调，由音频线程调用
    oboe::DataCallbackResult onAudioReady(
            oboe::AudioStream *oboeStream,
            void *audioData,
            int32_t numFrames) override;

private:
    std::shared_ptr<oboe::AudioStream> stream_;
    PaUtilRingBuffer ringBuffer_;
    // 环形缓冲区的数据存储区，大小根据延迟需求调整
    static constexpr int kRingBufferFrames = 4096; // ~85ms @48kHz
    int16_t ringBufferData_[kRingBufferFrames * 2]; // 立体声

    bool initRingBuffer();
};

#endif // AUDIO_ENGINE_H