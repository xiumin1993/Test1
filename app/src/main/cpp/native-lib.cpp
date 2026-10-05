#include <jni.h>
#include "audio_engine.h"
#include <android/log.h>

#define LOG_TAG "NativeLib"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// 全局唯一的 AudioEngine 实例
static AudioEngine gAudioEngine;

// 启动 Oboe 播放引擎
extern "C" JNIEXPORT void JNICALL
Java_com_example_test_MainActivity_startAudio(JNIEnv *env, jobject /* this */) {
    LOGI("startAudio called");
    gAudioEngine.start();
}

// 停止 Oboe 播放引擎
extern "C" JNIEXPORT void JNICALL
Java_com_example_test_MainActivity_stopAudio(JNIEnv *env, jobject /* this */) {
    LOGI("stopAudio called");
    gAudioEngine.stop();
}

// 写入 PCM 数据到环形缓冲区（供 Kotlin 层调用）
extern "C" JNIEXPORT jint JNICALL
Java_com_example_test_MainActivity_writeAudioData(JNIEnv *env, jobject /* this */,
                                                  jshortArray data, jint numFrames) {
    if (data == nullptr || numFrames <= 0) return 0;
    jshort *buf = env->GetShortArrayElements(data, nullptr);
    if (buf == nullptr) return 0;
    ring_buffer_size_t written = gAudioEngine.writeAudioData(buf, numFrames);
    env->ReleaseShortArrayElements(data, buf, JNI_ABORT);
    return static_cast<jint>(written);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_test_MainActivity_setVolume(JNIEnv *env, jobject /* this */, jfloat volume) {
    gAudioEngine.setVolume(volume);
}