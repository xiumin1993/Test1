cmake_minimum_required(VERSION 3.22.1)
project("test1")

include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/audio
        ${CMAKE_CURRENT_SOURCE_DIR}/ringbuffer
        ${CMAKE_CURRENT_SOURCE_DIR}/roc/include
)

find_package(oboe REQUIRED CONFIG)

# 导入预编译的 libroc.so
add_library(roc SHARED IMPORTED)
set_target_properties(roc PROPERTIES
        IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/../../jniLibs/${ANDROID_ABI}/libroc.so
)

add_library(
        native-lib
        SHARED
        jni/native-lib.cpp
        audio/audio_engine.cpp
        ringbuffer/pa_ringbuffer.c
        roc/roc_receiver.cpp
)

target_link_libraries(
        native-lib
        oboe::oboe
        roc
        log
        android
)