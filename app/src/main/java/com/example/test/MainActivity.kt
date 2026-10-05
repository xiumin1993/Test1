package com.example.test

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent

class MainActivity : ComponentActivity() {

    // ==================== JNI 声明 ====================
    external fun setVolume(volume: Float)
    external fun startRocReceiver(port: Int): Boolean
    external fun stopRocReceiver()

    companion object {
        init {
            System.loadLibrary("native-lib")
        }
    }

    // ==================== 系统音量同步 ====================
    private lateinit var volumeSync: SystemVolumeSync

    // ==================== 生命周期 ====================
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // 初始化音量同步：音量变化时调用 native 的 setVolume
        volumeSync = SystemVolumeSync(this) { volume ->
            setVolume(volume)
        }
        volumeSync.start()

        setContent {
            AudioScreen(
                onStart = { startRocReceiver(10001) },
                onStop = { stopRocReceiver() }
            )
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        volumeSync.stop()
    }
}