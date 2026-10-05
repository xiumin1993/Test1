package com.example.test

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent

class MainActivity : ComponentActivity() {

    // 声明 JNI 方法
    external fun startAudio()
    external fun stopAudio()

    companion object {
        init {
            // 这个只会在真实的 Android 设备/模拟器上执行
            System.loadLibrary("native-lib")
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            AudioScreen(
                onStart = { startAudio() },
                onStop = { stopAudio() }
            )
        }
    }
}