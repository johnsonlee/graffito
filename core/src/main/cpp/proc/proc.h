#ifndef PROC_H
#define PROC_H

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

pid_t procfs_get_tid(int (*select)(pid_t));

const char* procfs_get_thread_status(pid_t tid, char* line, size_t len, int (*select)(const char*));

const char* procfs_get_thread_name(pid_t tid, char* buf, size_t size);

int procfs_get_map_address(const char* path, uintptr_t* address);

#ifdef __cplusplus
}
#endif

#endif /* PROC_H */
