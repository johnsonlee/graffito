#ifndef FILE_H
#define FILE_H

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define fd_dev_null (file_open_dev_null())

int file_open_dev_null(void);

int64_t file_get_size(const char* path);

ssize_t file_read_fully(const char* path, char* content, size_t cap);

int64_t file_get_fd_size(int fd);

ssize_t file_read_fd_fully(int fd, char* content, size_t cap);

#ifdef __cplusplus
};
#endif

#endif /* FILE_H */
