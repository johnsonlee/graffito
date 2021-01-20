#ifndef APP_H
#define APP_H

#include <stddef.h>
#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

const char* get_cmdline(char* buf, size_t n);

const char* get_files_path(JNIEnv* env, char* buf, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* APP_H */
