package com.example.test

import android.os.Bundle
import android.os.Handler
import android.os.Looper
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.runtime.mutableStateOf
import java.util.Locale

class MainActivity : ComponentActivity() {

    // ==================== JNI 声明 ====================
    external fun setVolume(volume: Float)
    external fun startRocReceiver(port: Int): Boolean
    external fun stopRocReceiver()
    external fun getBufferLevelMs(): Float

    companion object {
        init {
            System.loadLibrary("native-lib")
        }
    }

    // ==================== UI 状态 ====================
    private val isRunningState = mutableStateOf(false)
    private val statusTextState = mutableStateOf("已停止")
    private val latencyTextState = mutableStateOf("-- ms")

    // ==================== 延迟显示定时器 ====================
    private val metricsHandler = Handler(Looper.getMainLooper())
    private val metricsRunnable = object : Runnable {
        override fun run() {
            if (!isRunningState.value) return

            val levelMs = getBufferLevelMs()
            latencyTextState.value = String.format(Locale.US, "%.0f ms", levelMs)

            // 每秒更新一次
            metricsHandler.postDelayed(this, 1000)
        }
    }

    // ==================== 音量同步 ====================
    private lateinit var volumeSync: SystemVolumeSync

    // ==================== 生命周期 ====================
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // 音量同步：立即读一次 + 监听后续变化
        volumeSync = SystemVolumeSync(this) { volume -> setVolume(volume) }
        volumeSync.start()

        setContent {
            AudioScreen(
                isRunning = isRunningState.value,
                statusText = statusTextState.value,
                latencyText = latencyTextState.value,
                onStart = { handleStart() },
                onStop = { handleStop() }
            )
        }
    }

    override fun onDestroy() {
        super.onDestroy()

        // 停止音量监听
        volumeSync.stop()

        // 停止延迟查询
        metricsHandler.removeCallbacks(metricsRunnable)

        // 若仍在运行，释放 native 资源
        if (isRunningState.value) {
            stopRocReceiver()
            isRunningState.value = false
        }
    }

    // ==================== 业务逻辑 ====================
    private fun handleStart() {
        // 防重入
        if (isRunningState.value) return

        statusTextState.value = "启动中..."
        val ok = startRocReceiver(10001)
        if (ok) {
            isRunningState.value = true
            statusTextState.value = "已启动，监听端口 10001"
            // 启动延迟查询定时器
            metricsHandler.post(metricsRunnable)
        } else {
            isRunningState.value = false
            statusTextState.value = "启动失败，请查看日志"
        }
    }

    private fun handleStop() {
        // 防重入
        if (!isRunningState.value) return

        statusTextState.value = "停止中..."
        stopRocReceiver()
        isRunningState.value = false
        statusTextState.value = "已停止"
        latencyTextState.value = "-- ms"

        // 停止延迟查询定时器
        metricsHandler.removeCallbacks(metricsRunnable)
    }
}