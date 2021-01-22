#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/eventfd.h>
#include <sys/syscall.h>

#include <jni.h>

#include "anr.h"
#include "app.h"
#include "defs.h"
#include "art.h"
#include "log.h"
#include "procfs.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunreachable-code"
#pragma clang diagnostic ignored "-Wgnu-statement-expression"

#ifdef __cplusplus
extern "C" {
#endif

#define TRACE_DIVIDER_LINE "----- pid %d at %s -----\n"

static JavaVM* jvm;
static sigset_t old_sigset;
static struct sigaction old_action;
static int fd_event = -1;

typedef void (*sigaction_t)(int, siginfo_t*, void*);

static void handler(int sig, siginfo_t* info, void* args) {
    UNUSED(sig);
    UNUSED(info);
    UNUSED(args);

    LOGD("caught signal %d", sig);
    if (fd_event >= 0) {
        uint64_t flag = 1;
        TEMP_FAILURE_RETRY(write(fd_event, &flag, sizeof(flag)));
    }
}

/**
 * Watch signal `SIGQUIT`
 */
static int watch(sigaction_t handler) {
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGQUIT);
    if (0 != pthread_sigmask(SIG_UNBLOCK, &sigset, &old_sigset)) {
        goto error;
    }

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    sigfillset(&act.sa_mask);
    act.sa_sigaction = handler;
    act.sa_flags = SA_RESTART | SA_SIGINFO;
    if (0 != sigaction(SIGQUIT, &act, &old_action)) {
        pthread_sigmask(SIG_SETMASK, &old_sigset, NULL);
        goto error;
    }

    return 0;

error:
    return errno;
}

static int open_trace_file(const char* dir) {
    char ymd[20];
    int64_t ts;
    struct tm* tm;
    struct timeval tv;

    gettimeofday(&tv, NULL);
    ts = ((tv.tv_sec * 1000L) + (tv.tv_usec / 1000L));

    if (NULL == (tm = localtime(&tv.tv_sec))) {
        goto error;
    }
    strftime(ymd, sizeof(ymd), "%Y-%m-%d %H:%M:%S", tm);

    char trace[PATH_MAX];
    snprintf(trace, sizeof(trace), "%s/trace-%"PRIi64".txt", dir, ts);

    int fd;
    int flags = O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC;
    mode_t mode = S_IRWXU | S_IRGRP | S_IROTH;
    if (-1 == (fd = TEMP_FAILURE_RETRY(open(trace, flags, mode)))) {
        LOGD("failed to open %s: %s", trace, strerror(errno));
        goto error;
    }

    LOGD("dump trace to %s", trace);
    dprintf(fd, TRACE_DIVIDER_LINE, getpid(), ymd);
    return fd;

error:
    return -1;
}

static int check_signal_catcher_status(const char* line) {
    return strncmp(line, "SigBlk:\t0000000000001000", 24);
}

static int select_signal_catcher(pid_t tid) {
    char name[16];
    procfs_get_task_comm(tid, name, sizeof(name));
    if (0 != strcmp("Signal Catcher", name)) {
        return -1;
    }

    char line[256];
    return NULL == procfs_get_thread_status(tid, line, sizeof(line), check_signal_catcher_status);
}

static void anr_rethrow(void) {
    static pid_t tid = -1;
    if (tid < 0) {
        if (0 > (tid = procfs_get_tid(select_signal_catcher))) {
            LOGD("Signal Catcher not found");
        }
    }

    LOGD("[Signal Catcher] tid=%d", tid);

    if (tid >= 0) {
        syscall(SYS_tgkill, getpid(), tid, SIGQUIT);
    }
}

/**
 * Dump ANR trace
 */
static void* anr_dumper(void* args) {
    UNUSED(args);

    JNIEnv* env = NULL;
    JavaVMAttachArgs attach_args = {
        .version = JNI_VERSION_1_6,
        .name = "TraceDumper",
        .group = NULL
    };

    pthread_detach(pthread_self());
    if (JNI_OK != (*jvm)->AttachCurrentThread(jvm, &env, &attach_args)) {
        goto exit;
    }

    // app files dir
    char files[PATH_MAX];
    get_files_path(env, files, sizeof(files));
    LOGD("files: %s", files);

    // app process name
    char cmdline[1024];
    get_cmdline(cmdline, sizeof(cmdline));
    LOGD("cmdline: %s", cmdline);

    int fd;
    uint64_t flag = 0;

    for (;;) {
        if (-1 == TEMP_FAILURE_RETRY(read(fd_event, &flag, sizeof(flag)))) {
            break;
        }

        if ((fd = open_trace_file(files)) < 0) {
            continue;
        }

        dprintf(fd, "Cmd line: %s\n", cmdline);

        if (dup2(fd, STDERR_FILENO) < 0) {
            LOGD("failed to redirect stderr to fd (%d)", fd);
            goto done;
        }

        LOGD("runtime dump start");
        if (0 != art_dump()) {
            LOGD("failed to dump ANR trace");
            goto done;
        }
        LOGD("runtime dump complete");

    done:
        fflush(NULL);
        dup2(fd_dev_null, STDERR_FILENO);
        close(fd);
        anr_rethrow();
    }

    LOGD("trace dumper quit");
    (*jvm)->DetachCurrentThread(jvm);

exit:
    close(fd_event);
    return NULL;
}

int anr_watch(JavaVM* vm) {
    static int watched = 0;
    if (watched) {
        goto done;
    }

    jvm = vm;

    if ((fd_event = eventfd(0, EFD_CLOEXEC)) < 0) {
        LOGD("eventfd: %s", strerror(errno));
        return errno != 0 ? errno : -1;
    }

    int rc;
    if (0 != (rc = watch(handler))) {
        LOGD("failed to watch ANR: %s", strerror(rc));
        goto cleanup;
    }

    pthread_t thread;
    if (0 != (rc = pthread_create(&thread, NULL, anr_dumper, NULL))) {
        goto cleanup;
    }

done:
    watched = 1;
    return 0;

cleanup:
    close(fd_event);
    fd_event = -1;
    return rc;
}

#ifdef __cplusplus
}
#endif

#pragma clang diagnostic pop
