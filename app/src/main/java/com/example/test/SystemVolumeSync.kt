package com.example.test

import android.content.Context
import android.database.ContentObserver
import android.media.AudioManager
import android.os.Handler
import android.os.Looper
import android.provider.Settings

class SystemVolumeSync(
    private val context: Context,
    private val onVolumeChanged: (Float) -> Unit
) {
    private val audioManager = context.getSystemService(Context.AUDIO_SERVICE) as AudioManager

    // 记录上一次下发的值；负数作为哨兵，保证首次（含重新订阅后）一定会同步
    private var lastVolume = -1f

    private val observer = object : ContentObserver(Handler(Looper.getMainLooper())) {
        override fun onChange(selfChange: Boolean) {
            syncToNative()
        }
    }

    fun start() {
        // 重置缓存，确保本次订阅的首次同步一定下发
        lastVolume = -1f
        // 立即同步一次
        syncToNative()
        // 注册监听
        context.contentResolver.registerContentObserver(
            Settings.System.CONTENT_URI, true, observer
        )
    }

    fun stop() {
        context.contentResolver.unregisterContentObserver(observer)
        // 解绑后不再信任缓存，下次 start() 时重新下发
        lastVolume = -1f
    }

    private fun syncToNative() {
        val max = audioManager.getStreamMaxVolume(AudioManager.STREAM_MUSIC)
        val current = audioManager.getStreamVolume(AudioManager.STREAM_MUSIC)
        val normalized = if (max > 0) current.toFloat() / max.toFloat() else 1.0f

        // 该监听绑定的是整个 Settings.System，任何系统设置变动都会回调到这里，
        // 但音乐音量未必变过，值相同就跳过，避免无谓的 JNI 调用
        if (normalized == lastVolume) return
        lastVolume = normalized

        onVolumeChanged(normalized)
    }
}