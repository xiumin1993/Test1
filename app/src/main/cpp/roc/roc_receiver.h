#ifndef ROC_RECEIVER_H
#define ROC_RECEIVER_H

#include <atomic>
#include <thread>
#include <cstdint>
#include <string>

class AudioEngine;

class RocReceiver {
public:
    RocReceiver();
    ~RocReceiver();

    /**
     * 启动 Roc 接收器
     * @param engine  目标 AudioEngine，接收到的 PCM 写入其环形缓冲区
     * @param port    监听端口，需与 Windows 端一致
     * @return true 表示启动成功
     */
    bool start(AudioEngine& engine, int port);

    /**
     * 停止 Roc 接收器
     */
    void stop();

private:
    void receiveThreadFunc();

    AudioEngine* engine_ = nullptr;
    std::atomic<bool> running_{false};
    std::thread thread_;
    int port_ = 10001;

    // Roc 对象
    struct roc_context* context_ = nullptr;
    struct roc_receiver* receiver_ = nullptr;
};

#endif // ROC_RECEIVER_H