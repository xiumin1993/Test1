cmake_minimum_required(VERSION 3.22.1)
project("test1")

# 查找 Oboe
find_package(oboe REQUIRED CONFIG)

# 添加所有源文件
add_library(
        native-lib
        SHARED
        native-lib.cpp
        audio_engine.cpp
        pa_ringbuffer.c
)

# 链接 Oboe 和系统库
target_link_libraries(
        native-lib
        oboe::oboe
        log
        android
)