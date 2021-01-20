#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <jni.h>

#include "anr.h"
#include "defs.h"
#include "art.h"
#include "log.h"
#include "linker.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __LP64__
    #define _64_ STRINGIFY(64)
#else
    #define _64_
#endif

#define LIB "lib"_64_

#define SYSTEM_LIB(so)       "/system/"LIB"/" # so
#define APEX_RUNTIME_LIB(so) "/apex/com.android.runtime/"LIB"/" # so
#define APEX_ART_LIB(so)     "/apex/com.android.art/"LIB"/" # so

/* legacy libraries */
#define LIBCPP               SYSTEM_LIB(libc++.so)
#define LIBART               SYSTEM_LIB(libart.so)

/* apex managed libraries */
#define APEX_LIBCPP          APEX_RUNTIME_LIB(libc++.so)
#define APEX_LIBART_29       APEX_RUNTIME_LIB(libart.so)
#define APEX_LIBART_30       APEX_ART_LIB(libart.so)

/**
 * std::cerr
 */
#define LIBCPP_CERR                      "_ZNSt3__14cerrE"

/**
 * art::Runtime::instance_
 */
#define LIBART_RUNTIME_INSTANCE          "_ZN3art7Runtime9instance_E"

/**
 * art::Runtime::DumpForSigQuit(std::ostream&)
 */
#define LIBART_RUNTIME_DUMP_FOR_SIGQUIT  "_ZN3art7Runtime14DumpForSigQuitERNSt3__113basic_ostreamIcNS1_11char_traitsIcEEEE"

/**
 * art::Dbg::SuspendVM()
 */
#define LIBART_DBG_SUSPEND_VM            "_ZN3art3Dbg9SuspendVMEv"

/**
 * art::Dbg::ResumeVM()
 */
#define LIBART_DBG_RESUME_VM             "_ZN3art3Dbg8ResumeVMEv"

#define LOLLIPOP (runtime.api_level >= 21 && runtime.api_level <= 22)

typedef void (*DumpForSigQuit)(void* runtime, void* ostream);
typedef void (*SuspendVM)(void);
typedef void (*ResumeVM)(void);

typedef struct runtime {
    void** instance;
    void* cerr;
    struct {
        DumpForSigQuit dumpForSigQuit;
        SuspendVM suspendVM;
        ResumeVM resumeVM;
    };
    int api_level;
} runtime_t;

static runtime_t runtime;

int art_init(void) {
    static int initialized = 0;
    if (0 != initialized) {
        return 0;
    }

    char pathname[128];
    shared_library_t* libcpp = NULL;
    shared_library_t* libart = NULL;
    int rc = -1;

    memset(&runtime, 0, sizeof(runtime));
    runtime.api_level = android_get_device_api_level();

    // load c++.so
    if (runtime.api_level >= 29) {
        if (NULL == (libcpp = shared_library_open(APEX_LIBCPP))) {
            LOGD("cannot load "APEX_LIBCPP);
        }
    }
    if (NULL == libcpp && NULL == (libcpp = shared_library_open(LIBCPP))) {
        LOGD("cannot load "LIBCPP);
        goto art;
    }

    shared_library_get_pathname(libcpp, pathname, sizeof(pathname));

    // load cerr
    if (NULL == (runtime.cerr = shared_library_lookup(libcpp, LIBCPP_CERR))) {
        LOGD("cannot load symbol "LIBCPP_CERR" from %s", pathname);
        goto art;
    }

art: // load art.so
    if (runtime.api_level >= 30) {
        if (NULL == (libart = shared_library_open(APEX_LIBART_30))) {
            LOGD("cannot load "APEX_LIBART_30);
        }
    } else if (runtime.api_level >= 29) {
        if (NULL == (libart = shared_library_open(APEX_LIBART_29))) {
            LOGD("cannot load "APEX_LIBART_29);
        }
    }
    if (NULL == libart && NULL == (libart = shared_library_open(LIBART))) {
        LOGD("cannot load "LIBART);
        goto cleanup;
    }

    shared_library_get_pathname(libart, pathname, sizeof(pathname));

    if (NULL == (runtime.instance = (void**) shared_library_lookup(libart, LIBART_RUNTIME_INSTANCE))) {
        LOGD("cannot load symbol "LIBART_RUNTIME_INSTANCE" from %s", pathname);
        goto cleanup;
    }
    if (NULL == (runtime.dumpForSigQuit = (DumpForSigQuit) shared_library_lookup(libart, LIBART_RUNTIME_DUMP_FOR_SIGQUIT))) {
        LOGD("cannot load symbol "LIBART_RUNTIME_DUMP_FOR_SIGQUIT" from %s", pathname);
        goto cleanup;
    }
    // Android Lollipop
    if (LOLLIPOP) {
        if (NULL == (runtime.resumeVM = (ResumeVM) shared_library_lookup(libart, LIBART_DBG_SUSPEND_VM))) {
            LOGD("cannot load symbol "LIBART_DBG_SUSPEND_VM" from %s", pathname);
            goto cleanup;
        }
        if (NULL == (runtime.suspendVM = (SuspendVM) shared_library_lookup(libart, LIBART_DBG_RESUME_VM))) {
            LOGD("cannot load symbol "LIBART_DBG_RESUME_VM" from %s", pathname);
            goto cleanup;
        }
    }

    LOGD(" std::cerr                       %"PRIxPTR, (uintptr_t) runtime.cerr);
    LOGD(" art::Runtime::instance          %"PRIxPTR, (uintptr_t) runtime.instance);
    LOGD("*art::Runtime::instance          %"PRIxPTR, (uintptr_t) *runtime.instance);
    LOGD(" art::Runtime::DumpForSigQuit    %"PRIxPTR, (uintptr_t) runtime.dumpForSigQuit);
    LOGD(" art::Dbg::SuspendVM             %"PRIxPTR, (uintptr_t) runtime.suspendVM);
    LOGD(" art::Dbg::ResumeVM              %"PRIxPTR, (uintptr_t) runtime.resumeVM);

    rc = 0;
    initialized = 1;

cleanup:
    if (NULL != libcpp) {
        shared_library_close(&libcpp);
    }
    if (NULL != libart) {
        shared_library_close(&libart);
    }
    return rc;
}

int art_dump(void) {
    if (LOLLIPOP && NULL != runtime.suspendVM) {
        runtime.suspendVM();
    }

    if (NULL != runtime.dumpForSigQuit && NULL != *runtime.instance && NULL != runtime.cerr) {
        runtime.dumpForSigQuit(*runtime.instance, runtime.cerr);
    }

    if (LOLLIPOP && NULL != runtime.resumeVM) {
        runtime.resumeVM();
    }
    return 0;
}

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    UNUSED(reserved);

    if (NULL == vm) {
        return JNI_ERR;
    }

    JNIEnv* env;
    if (JNI_OK != (*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_6) || NULL == env || NULL == *env) {
        return JNI_ERR;
    }

    if (0 != art_init() || 0 != anr_watch(vm)) {
        return JNI_ERR;
    }

    return JNI_VERSION_1_6;
}

#ifdef __cplusplus
}
#endif

#pragma clang diagnostic pop
