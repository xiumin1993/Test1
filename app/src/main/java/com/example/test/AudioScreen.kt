package com.example.test

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.example.test.ui.theme.TestTheme

@Composable
fun AudioScreen(
    onStart: () -> Unit,
    onStop: () -> Unit
) {
    Column(modifier = Modifier.padding(16.dp)) {
        Text("Android Audio Receiver")
        Spacer(modifier = Modifier.height(16.dp))
        Button(onClick = onStart) {
            Text("Start Audio Engine")
        }
        Spacer(modifier = Modifier.height(8.dp))
        Button(onClick = onStop) {
            Text("Stop Audio Engine")
        }
    }
}

@Preview(showBackground = true)
@Composable
fun AudioScreenPreview() {
    TestTheme {
        // 预览时传入空实现，避免调用 native 方法
        AudioScreen(onStart = {}, onStop = {})
    }
}