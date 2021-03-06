#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <linux/sched.h>
#include <sys/param.h>

#include "defs.h"
#include "log.h"
#include "procfs.h"
#include "text.h"

#ifndef TASK_COMM_LEN
#define TASK_COMM_LEN 24
#endif

#define MAPS_FMT "%"SCNxPTR"-%*"SCNxPTR" %*4s %"SCNxPTR" %*x:%*x %*d%n"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"

#ifdef __cplusplus
extern "C" {
#endif

const char* procfs_get_task_comm(pid_t tid, char* buf, size_t size) {
    if (NULL == buf) {
        return NULL;
    }

    char path[64];
    snprintf(path, sizeof(path), "/proc/self/task/%d/comm", tid);
    char comm[TASK_COMM_LEN];
    ssize_t len = file_read_fully(path, comm, sizeof(comm));
    if (len < 0) {
        return NULL;
    }

    strncpy(buf, comm, MIN((size_t) len, size));
    return strrtrim(buf);
}

pid_t procfs_get_tid(int (*select)(pid_t)) {
    DIR *dir;
    char task[128];
    pid_t tid = -1;
    struct dirent *ent;

    snprintf(task, sizeof(task), "/proc/%d/task", getpid());
    if (NULL == (dir = opendir(task))) {
        goto error;
    }

    while (NULL != (ent = readdir(dir))) {
        for (char* c = ent->d_name; *c != '\0'; c++) {
            if (!isdigit(*c)) goto outer;
        }

        errno = 0;
        if (0 >= (tid = (pid_t) strtol(ent->d_name, NULL, 10)) || 0 != errno) {
            continue;
        }

        if (0 == select(tid)) {
            break;
        }

        tid = -1;
        outer: continue;
    }

    closedir(dir);

error:
    return tid;
}

const char* procfs_get_thread_status(pid_t tid, char* line, size_t len, int (*select)(const char*)) {
    char status[128];
    snprintf(status, sizeof(status), "/proc/%d/status", tid);

    FILE* fp = NULL;
    if (NULL == (fp = fopen(status, "r"))) {
        return NULL;
    }

    while (fgets(line, (int) len, fp)) {
        if (0 == select(line)) {
            fclose(fp);
            return line;
        }
    }

    return NULL;
}

/**
 * Locate shared library from /proc/self/maps
 *
 * <pre>
 * address               perm offset   dev   inode                          pathname
 * 7340f11000-7340ff3000 r-xp 00000000 fd:00 1951                           /system/lib64/libc++.so
 * 7340ff4000-7340ffc000 r--p 000e2000 fd:00 1951                           /system/lib64/libc++.so
 * 7340ffc000-7340ffd000 rw-p 000ea000 fd:00 1951                           /system/lib64/libc++.so
 * </pre>
 */
int procfs_get_map_address(const char* pathname, uintptr_t* address) {
    FILE* maps = NULL;
    if (NULL == (maps = fopen("/proc/self/maps", "r"))) {
        LOGD("cannot open /proc/self/maps : %s", strerror(errno));
        return errno;
    }

    char line[PATH_MAX];
    uintptr_t offset;
    int pos;
    char* str;
    int found = 0;

    while (fgets(line, sizeof(line), maps)) {
        if (2 != sscanf(line, MAPS_FMT, address, &offset, &pos) || 0 != offset) {
            continue;
        }
        str = strtrim(line + pos);
        if (0 == strcmp(str, pathname)) {
            LOGD("%s", line);
            found = 1;
            break;
        }
    }

    fclose(maps);
    return !found;

}

#ifdef __cplusplus
}
#endif

#pragma clang diagnostic pop
