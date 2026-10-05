package com.example.test

import android.content.Context
import android.database.ContentObserver
import android.media.AudioManager
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.provider.Settings
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import kotlin.math.PI
import kotlin.math.sin

class MainActivity : ComponentActivity() {

    // ==================== JNI 声明（正式功能） ====================
    external fun startAudio()
    external fun stopAudio()
    external fun writeAudioData(data: ShortArray, numFrames: Int): Int
    external fun setVolume(volume: Float)

    companion object {
        init {
            System.loadLibrary("native-lib")
        }
    }

    // ====================================================================
    // ============ 系统音量同步（正式功能） ==============================
    // ====================================================================

    private lateinit var audioManager: AudioManager

    /**
     * 监听系统媒体音量变化，实时同步到 native 层
     */
    private val volumeObserver = object : ContentObserver(Handler(Looper.getMainLooper())) {
        override fun onChange(selfChange: Boolean) {
            syncSystemVolumeToNative()
        }
    }

    /**
     * 读取当前系统媒体音量，归一化到 [0.0, 1.0]，写入 native 层
     */
    private fun syncSystemVolumeToNative() {
        val max = audioManager.getStreamMaxVolume(AudioManager.STREAM_MUSIC)
        val current = audioManager.getStreamVolume(AudioManager.STREAM_MUSIC)
        val normalized = if (max > 0) current.toFloat() / max.toFloat() else 1.0f
        setVolume(normalized)
    }

    // ====================================================================
    // ============ 测试音控制字段：仅测试用，接入 Roc 后删除 ============
    // ====================================================================
    @Volatile private var toneRunning = false
    private var toneThread: Thread? = null
    // ====================================================================


    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // ============ 音频管理器与音量同步（正式功能） ============
        audioManager = getSystemService(Context.AUDIO_SERVICE) as AudioManager

        // 1. 立即同步一次系统音量到 native 层，保证第一次播放就是正确的音量
        syncSystemVolumeToNative()

        // 2. 注册监听，后续用户按键调节音量时会实时同步
        contentResolver.registerContentObserver(
            Settings.System.CONTENT_URI,
            true,
            volumeObserver
        )
        // ==========================================================

        setContent {
            AudioScreen(
                onStart = { startTestTone() },   // 测试用
                onStop  = { stopTestTone() }     // 测试用
                // 接入 Roc 后改成：
                // onStart = { startAudio() },
                // onStop  = { stopAudio() }
            )
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        // 注销音量监听，防止内存泄漏
        contentResolver.unregisterContentObserver(volumeObserver)
    }


    // ====================================================================
    // ============ 以下是测试音生成逻辑：仅测试用 ========================
    // ============ TODO: 接入 Roc Toolkit 后，删除本区块全部代码 ==========
    // ====================================================================

    private fun startTestTone() {
        syncSystemVolumeToNative()

        // 1. 启动 Oboe 播放引擎
        startAudio()

        // 2. 避免重复启动
        if (toneRunning) return
        toneRunning = true

        // 3. 启动后台线程，持续生成正弦波并推入 native 层
        toneThread = Thread {
            val sampleRate = 48000
            val channelCount = 2
            val frequency = 440.0
            val framesPerWrite = 480          // 10ms @48kHz
            val amplitude = 16384.0
            val phaseInc = 2.0 * PI * frequency / sampleRate

            var phase = 0.0
            val buffer = ShortArray(framesPerWrite * channelCount)

            while (toneRunning) {
                for (i in 0 until framesPerWrite) {
                    val s = (sin(phase) * amplitude).toInt().toShort()
                    buffer[i * 2]     = s
                    buffer[i * 2 + 1] = s
                    phase += phaseInc
                    if (phase >= 2.0 * PI) phase -= 2.0 * PI
                }

                val written = writeAudioData(buffer, framesPerWrite)

                try {
                    if (written == framesPerWrite) {
                        Thread.sleep(8)
                    } else {
                        Thread.sleep(5)
                    }
                } catch (e: InterruptedException) {
                    break
                }
            }
        }.also { it.start() }
    }

    private fun stopTestTone() {
        toneRunning = false
        toneThread?.join()
        toneThread = null
        stopAudio()
    }

    // ====================================================================
    // ============ 测试音代码结束 ========================================
    // ====================================================================
}