#include <jni.h>
#include "audio_engine.h"
#include "roc/roc_receiver.h"
#include <android/log.h>

#define LOG_TAG "NativeLib"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// ==================== 全局实例 ====================
static AudioEngine gAudioEngine;
static RocReceiver* gRocReceiver = nullptr;

// ==================== 音量控制 ====================
extern "C" JNIEXPORT void JNICALL
Java_com_example_test_MainActivity_setVolume(JNIEnv *env, jobject /* this */, jfloat volume) {
    LOGI("setVolume called: %f", volume);
    gAudioEngine.setVolume(volume);
}

// ==================== Roc 接收器控制 ====================
extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_test_MainActivity_startRocReceiver(JNIEnv *env, jobject /* this */, jint port) {
    LOGI("startRocReceiver called, port=%d", port);

    if (gRocReceiver == nullptr) {
        gRocReceiver = new RocReceiver();
    }

    // 1. 先启动 Oboe 播放引擎（从环形缓冲区读取）
    if (!gAudioEngine.start()) {
        LOGI("Failed to start AudioEngine");
        return JNI_FALSE;
    }

    // 2. 再启动 Roc 接收器（向环形缓冲区写入）
    bool ok = gRocReceiver->start(gAudioEngine, port);
    if (!ok) {
        LOGI("Failed to start RocReceiver");
        gAudioEngine.stop();
        return JNI_FALSE;
    }

    LOGI("RocReceiver started successfully");
    return JNI_TRUE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_test_MainActivity_stopRocReceiver(JNIEnv *env, jobject /* this */) {
    LOGI("stopRocReceiver called");

    if (gRocReceiver != nullptr) {
        gRocReceiver->stop();
    }
    gAudioEngine.stop();
}

extern "C" JNIEXPORT jfloat JNICALL
Java_com_example_test_MainActivity_getBufferLevelMs(JNIEnv *env, jobject /* this */) {
    return static_cast<jfloat>(gAudioEngine.getBufferLevelMs());
}