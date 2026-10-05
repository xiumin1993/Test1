package com.example.test

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.example.test.ui.theme.TestTheme

@Composable
fun AudioScreen(
    isRunning: Boolean,
    statusText: String,
    latencyText: String,
    onStart: () -> Unit,
    onStop: () -> Unit
) {
    Column(modifier = Modifier.padding(16.dp)) {
        Text(
            text = "Android Audio Receiver",
            style = MaterialTheme.typography.titleLarge
        )

        Spacer(Modifier.height(24.dp))

        // 状态显示
        Text(
            text = "状态: $statusText",
            style = MaterialTheme.typography.bodyLarge,
            color = if (isRunning) Color(0xFF2E7D32) else Color(0xFF757575)
        )

        Spacer(Modifier.height(8.dp))

        // 延迟/缓冲占用显示
        Text(
            text = "缓冲占用: $latencyText",
            style = MaterialTheme.typography.bodyLarge,
            color = Color(0xFF1976D2)
        )

        Spacer(Modifier.height(24.dp))

        // Start 按钮：只在未运行时可用
        Button(
            onClick = onStart,
            enabled = !isRunning,
            modifier = Modifier.fillMaxWidth()
        ) {
            Text(if (isRunning) "Running..." else "Start Audio Engine")
        }

        Spacer(Modifier.height(8.dp))

        // Stop 按钮：只在运行时可用
        Button(
            onClick = onStop,
            enabled = isRunning,
            modifier = Modifier.fillMaxWidth()
        ) {
            Text("Stop Audio Engine")
        }
    }
}

// ==================== 预览 ====================

@Preview(showBackground = true)
@Composable
fun AudioScreenPreview_Stopped() {
    TestTheme {
        AudioScreen(
            isRunning = false,
            statusText = "已停止",
            latencyText = "-- ms",
            onStart = {},
            onStop = {}
        )
    }
}

@Preview(showBackground = true)
@Composable
fun AudioScreenPreview_Running() {
    TestTheme {
        AudioScreen(
            isRunning = true,
            statusText = "已启动，监听端口 10001",
            latencyText = "24 ms",
            onStart = {},
            onStop = {}
        )
    }
}