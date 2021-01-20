#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <android/log.h>

#include <jni.h>

#include "app.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"

#ifdef __cplusplus
extern "C" {
#endif

#define CLASS_STRING          "java/lang/String"
#define CLASS_FILE            "java/io/File"
#define CLASS_APPLICATION     "android/app/Application"
#define CLASS_ACTIVITY_THREAD "android/app/ActivityThread"
#define CLASS_CONTEXT         "android/content/Context"

static jobject get_application(JNIEnv* env) {
    jclass class_activity_thread = (*env)->FindClass(env, CLASS_ACTIVITY_THREAD);
    jmethodID method_current_application = (*env)->GetStaticMethodID(env, class_activity_thread, "currentApplication", "()L"CLASS_APPLICATION";");
    return (*env)->CallStaticObjectMethod(env, class_activity_thread, method_current_application);
}

static jobject get_files_dir(JNIEnv* env) {
    jobject app = get_application(env);
    jclass class_context = (*env)->FindClass(env, CLASS_CONTEXT);
    jmethodID method_get_files_dir = (*env)->GetMethodID(env, class_context, "getFilesDir", "()L"CLASS_FILE";");
    return (*env)->CallObjectMethod(env, app, method_get_files_dir);
}

const char* get_files_path(JNIEnv* env, char* buf, size_t n) {
    jobject files = get_files_dir(env);
    jclass class_file = (*env)->FindClass(env, CLASS_FILE);
    jmethodID method_get_canonical_path = (*env)->GetMethodID(env, class_file, "getCanonicalPath", "()L"CLASS_STRING";");
    jstring canonical_path = (*env)->CallObjectMethod(env, files, method_get_canonical_path);
    const char* path = (*env)->GetStringUTFChars(env, canonical_path, NULL);
    snprintf(buf, n, "%s", path);
    (*env)->ReleaseStringUTFChars(env, canonical_path, path);
    return buf;
}


const char* get_cmdline(char* buf, size_t n) {
    char cmdline[1024];
    snprintf(cmdline, sizeof(cmdline), "/proc/%d/cmdline", getpid());

    int fd = TEMP_FAILURE_RETRY(open(cmdline, O_RDONLY));
    if (fd < 0) {
        return NULL;
    }

    TEMP_FAILURE_RETRY(read(fd, buf, n));
    TEMP_FAILURE_RETRY(close(fd));

    // trim tailing whitespace
    char* end = buf + strlen(buf) - 1;
    while (end > buf && isspace((unsigned char) *end)) end--;
    end[1] = '\0';

    return buf;
}

#ifdef __cplusplus
}
#endif

#pragma clang diagnostic pop
