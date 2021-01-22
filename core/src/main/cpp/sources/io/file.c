#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>

#include "file.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"

#ifdef __cplusplus
extern "C" {
#endif

int file_open_dev_null() {
    static int fd = -1;
    return fd > -1 ? fd : (fd = TEMP_FAILURE_RETRY(open("/dev/null", O_RDWR)));
}


int64_t file_get_size(const char* path) {
    int fd = TEMP_FAILURE_RETRY(open(path, O_RDONLY | O_CLOEXEC));
    if (fd < 0) {
        return -1;
    }

    int64_t size = file_get_fd_size(fd);
    close(fd);
    return size;
}

ssize_t file_read_fully(const char* path, char* content, size_t cap) {
    int fd = TEMP_FAILURE_RETRY(open(path, O_RDONLY | O_CLOEXEC));
    if (fd < 0) {
        return -1;
    }

    ssize_t n = file_read_fd_fully(fd, content, cap);
    close(fd);
    return n;
}

int64_t file_get_fd_size(int fd) {
    struct stat stat;

    if (fd > 0 && -1 != fstat(fd, &stat)) {
        return stat.st_size;
    }

    return -1;
}

ssize_t file_read_fd_fully(int fd, char* content, size_t cap) {
    size_t size = MAX((size_t) file_get_fd_size(fd), cap);
    return TEMP_FAILURE_RETRY(read(fd, content, size));
}

#ifdef __cplusplus
};
#endif

#pragma clang diagnostic pop
