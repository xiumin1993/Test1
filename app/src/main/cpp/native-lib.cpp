#include <jni.h>
#include <oboe/Oboe.h>
#include <android/log.h>
#include <cstring>

#define LOG_TAG "OboeEngine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

class AudioEngine : public oboe::AudioStreamDataCallback {
public:
    oboe::DataCallbackResult onAudioReady(
            oboe::AudioStream *oboeStream,
            void *audioData,
            int32_t numFrames) override {

        // TODO: 之后这里会从 Roc 的环形缓冲区读取数据
        // 现在先播放静音，验证音频流是否正常
        auto *output = static_cast<int16_t *>(audioData);
        memset(output, 0, numFrames * oboeStream->getChannelCount() * sizeof(int16_t));

        return oboe::DataCallbackResult::Continue;
    }
};

static AudioEngine engine;
static std::shared_ptr<oboe::AudioStream> stream;

extern "C" JNIEXPORT void JNICALL
Java_com_example_test_MainActivity_startAudio(JNIEnv *env, jobject /* this */) {
oboe::AudioStreamBuilder builder;
oboe::Result result = builder.setDirection(oboe::Direction::Output)
        ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
        ->setSharingMode(oboe::SharingMode::Exclusive)
        ->setFormat(oboe::AudioFormat::I16)
        ->setChannelCount(2)
        ->setSampleRate(48000)
        ->setDataCallback(&engine)
        ->openStream(stream);

if (result != oboe::Result::OK) {
LOGI("Failed to open stream: %s", oboe::convertToText(result));
return;
}

stream->requestStart();
LOGI("Oboe stream started");
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_test_MainActivity_stopAudio(JNIEnv *env, jobject /* this */) {
if (stream) {
stream->stop();
stream->close();
stream.reset();
LOGI("Oboe stream stopped");
}
}