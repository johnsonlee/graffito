#ifndef LOG_H
#define LOG_H

#include "defs.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"

#define TAG __FILE_NAME__":"STRINGIFY(__LINE__)

#if !(defined(NDEBUG))
    #ifdef __ANDROID__
        #include <android/log.h>
        #define LOGV(fmt, ...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, fmt, ##__VA_ARGS__)
        #define LOGD(fmt, ...) __android_log_print(ANDROID_LOG_DEBUG,   TAG, fmt, ##__VA_ARGS__)
        #define LOGI(fmt, ...) __android_log_print(ANDROID_LOG_INFO,    TAG, fmt, ##__VA_ARGS__)
        #define LOGW(fmt, ...) __android_log_print(ANDROID_LOG_WARN,    TAG, fmt, ##__VA_ARGS__)
        #define LOGE(fmt, ...) __android_log_print(ANDROID_LOG_ERROR,   TAG, fmt, ##__VA_ARGS__)
    #else
        #include <stdio.h>
        #define LOGV(fmt, ...) printf(fmt, ##__VA_ARGS__)
        #define LOGD(fmt, ...) printf(fmt, ##__VA_ARGS__)
        #define LOGI(fmt, ...) printf(fmt, ##__VA_ARGS__)
        #define LOGW(fmt, ...) printf(fmt, ##__VA_ARGS__)
        #define LOGE(fmt, ...) printf(fmt, ##__VA_ARGS__)
    #endif
#else
    #define LOGV(fmt, ...) VOID(fmt, ##__VA_ARGS__)
    #define LOGD(fmt, ...) VOID(fmt, ##__VA_ARGS__)
    #define LOGI(fmt, ...) VOID(fmt, ##__VA_ARGS__)
    #define LOGW(fmt, ...) VOID(fmt, ##__VA_ARGS__)
    #define LOGE(fmt, ...) VOID(fmt, ##__VA_ARGS__)
#endif

#pragma clang diagnostic pop

#endif /* LOG_H */
