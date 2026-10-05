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

    private val observer = object : ContentObserver(Handler(Looper.getMainLooper())) {
        override fun onChange(selfChange: Boolean) {
            syncToNative()
        }
    }

    fun start() {
        // 立即同步一次
        syncToNative()
        // 注册监听
        context.contentResolver.registerContentObserver(
            Settings.System.CONTENT_URI, true, observer
        )
    }

    fun stop() {
        context.contentResolver.unregisterContentObserver(observer)
    }

    private fun syncToNative() {
        val max = audioManager.getStreamMaxVolume(AudioManager.STREAM_MUSIC)
        val current = audioManager.getStreamVolume(AudioManager.STREAM_MUSIC)
        val normalized = if (max > 0) current.toFloat() / max.toFloat() else 1.0f
        onVolumeChanged(normalized)
    }
}