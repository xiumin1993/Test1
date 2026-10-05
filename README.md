# Test1

Android 端网络音频接收器（Demo）：PC 通过 Roc Toolkit 把音频打成 RTP 流发到手机，手机接收后经 Oboe 低延迟播放。发送端在 PC 上，不在本仓库。

## 架构

```
Kotlin/Compose  MainActivity（UI 状态/定时器） · AudioScreen · SystemVolumeSync（跟随系统音量）
      │ JNI (native-lib.cpp)
C++ Native      RocReceiver（收包线程）→ PaUtilRingBuffer → AudioEngine（Oboe 播放 + 音量缩放）
```

## 端口约定

| 端口 | 用途 | 协议 |
|------|------|------|
| 10001 | 音频 source | `RTP + RS8M`（FEC 编码的数据包） |
| 10002 | FEC repair | `RS8M`（修包） |

接收端绑定 `0.0.0.0`，需与 PC 端发送命令中的端口一致。

## 使用

1. 手机与 PC 处于同一局域网，启动 App 点击 **Start Audio Engine**（监听 10001/10002）。
2. PC 端安装 roc-toolkit 后发送：

```bash
# <手机IP> 换成手机实际地址
roc-send -v \
  -s rtp+rs8m://<手机IP>:10001 \
  -r rs8m://<手机IP>:10002 \
  input.wav
```

## 关键参数

| 参数 | 值 | 位置 |
|------|-----|------|
| 采样格式 | 48kHz / 立体声 / Float32 | `audio_engine.h`、`roc_receiver.cpp` |
| Roc target_latency | 50ms（50000000 ns） | `roc_receiver.cpp` |
| 环形缓冲容量 | 16384 帧 = 341.3ms @48kHz（2 的幂，溢出上限非实际延迟） | `audio_engine.h` |
| Oboe 配置 | LowLatency + Exclusive，缓冲 = 2×framesPerBurst（≈10ms） | `audio_engine.cpp` |

## 构建

- Android Studio 直接打开；NDK 30.x + CMake 3.22.1，Oboe 经 Prefab 引入
- 预编译 `libroc.so` 位于 `app/src/main/jniLibs/arm64-v8a/`，仅支持 arm64-v8a
- minSdk 24 / target+compileSdk 37
