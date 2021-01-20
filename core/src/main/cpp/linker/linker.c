#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <elf.h>
#include <fcntl.h>
#include <link.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "log.h"
#include "linker.h"
#include "proc.h"
#include "queue.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wgnu-statement-expression"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct shared_library_symbol {
    /* SYMTAB section header */
    size_t sym_offset;
    size_t sym_size;
    size_t sym_entsize;

    /* STRTAB section header */
    size_t str_offset;
    size_t str_size;
    size_t str_entsize;

    TAILQ_ENTRY(shared_library_symbol,) link;
} shared_library_symbol_t;

typedef TAILQ_HEAD(shared_library_symbols, shared_library_symbol,) shared_library_symbols_t;

struct shared_library {
    uintptr_t address;
    uintptr_t load_bias;
    uint8_t* data;
    size_t size;
    shared_library_symbols_t symbols;
    char pathname[PATH_MAX];
    int fd;
};

static int shared_library_mmap(shared_library_t* thiz) {
    struct stat stat;

    if (0 > (thiz->fd = TEMP_FAILURE_RETRY(open(thiz->pathname, O_RDONLY | O_CLOEXEC)))) {
        LOGD("failed to open %s: %s", thiz->pathname, strerror(errno));
        return errno;
    }

    if (0 != TEMP_FAILURE_RETRY(fstat(thiz->fd, &stat)) || stat.st_size <= 0) {
        LOGD("failed to stat %s: %s", thiz->pathname, strerror(errno));
        return errno;
    }

    thiz->size = (size_t) stat.st_size;

    if (MAP_FAILED == (thiz->data = (uint8_t*) mmap(NULL, thiz->size, PROT_READ, MAP_PRIVATE, thiz->fd, 0))) {
        LOGD("failed to mmap %s: %s", thiz->pathname, strerror(errno));
        return errno;
    }

    return 0;
}

static void* shared_library_get(shared_library_t* thiz, uintptr_t offset, size_t size) {
    if (offset + size > thiz->size) {
        return NULL;
    }
    return (void*) (thiz->data + offset);
}

static const char* shared_library_get_string(shared_library_t* thiz, uintptr_t offset) {
    uint8_t* ptr = thiz->data + offset;
    uint8_t* end = thiz->data + thiz->size;

    while (ptr < end) {
        if (*ptr == '\0') {
            return (char*) (thiz->data + offset);
        }
        ptr++;
    }

    return NULL;
}

static int shared_library_load(shared_library_t* thiz) {
    ElfW(Ehdr)* ehdr;
    ElfW(Phdr)* phdr;
    ElfW(Shdr)* shdr;
    ElfW(Shdr)* str_shdr;
    shared_library_symbol_t* symbol;

    // lookup ELF header
    if (NULL == (ehdr = shared_library_get(thiz, 0, sizeof(ElfW(Ehdr))))) {
        LOGD("failed to locate ELF header of %s", thiz->pathname);
        return -1;
    }

    // lookup load_bias
    for (size_t i = 0; i < ehdr->e_phnum * ehdr->e_phentsize; i += ehdr->e_phentsize) {
        if (NULL == (phdr = shared_library_get(thiz, ehdr->e_phoff + i, sizeof(ElfW(Phdr))))) {
            return -1;
        }

        if ((PT_LOAD == phdr->p_type) && (phdr->p_flags & PF_X) && (0 == phdr->p_offset)) {
            thiz->load_bias = phdr->p_vaddr;
            break;
        }
    }

    // lookup symbol table
    for (size_t i = ehdr->e_shentsize; i < ehdr->e_shnum * ehdr->e_shentsize; i += ehdr->e_shentsize) {
        if (NULL == (shdr = shared_library_get(thiz, ehdr->e_shoff + i, sizeof(ElfW(Shdr))))) {
            return -1;
        }

        if ((SHT_SYMTAB == shdr->sh_type) || (SHT_DYNSYM == shdr->sh_type)) {
            if (shdr->sh_link >= ehdr->e_shnum) {
                continue;
            }

            if (NULL == (str_shdr = shared_library_get(thiz, ehdr->e_shoff + shdr->sh_link * ehdr->e_shentsize, sizeof(ElfW(Shdr))))) {
                return -1;
            }

            if (SHT_STRTAB != str_shdr->sh_type) {
                continue;
            }

            if (NULL == (symbol = calloc(1, sizeof(shared_library_symbol_t)))) {
                return ENOMEM;
            }

            symbol->sym_offset = shdr->sh_offset;
            symbol->sym_size = shdr->sh_size;
            symbol->sym_entsize = shdr->sh_entsize;

            symbol->str_offset = str_shdr->sh_offset;
            symbol->str_size = str_shdr->sh_size;
            symbol->str_entsize = str_shdr->sh_entsize;

            TAILQ_INSERT_TAIL(&(thiz->symbols), symbol, link);
        }
    }

    if (thiz->symbols.tqh_first == NULL && thiz->symbols.tqh_last == NULL) {
        return -1;
    }

    return 0;
}

shared_library_t* shared_library_open(const char* pathname) {
    shared_library_t* lib = calloc(1, sizeof(shared_library_t));
    if (NULL == lib) {
        return NULL;
    }

    lib->fd = -1;
    lib->data = MAP_FAILED;
    strcpy(lib->pathname, pathname);
    TAILQ_INIT(&(lib->symbols));

    if (procfs_get_map_address(pathname, &lib->address)
            || shared_library_mmap(lib)
            || shared_library_load(lib)) {
        goto error;
    }

    return lib;

error:
    shared_library_close(&lib);
    return NULL;
}

const char* shared_library_get_pathname(shared_library_t* thiz, char* buf, size_t len) {
    if (NULL == thiz || NULL == buf) {
        return NULL;
    }
    strncpy(buf, thiz->pathname, len);
    return buf;
}

uintptr_t shared_library_get_address(shared_library_t* thiz) {
    return thiz->address;
}

void shared_library_close(shared_library_t** thiz) {
    if (NULL == thiz || NULL == *thiz) {
        return;
    }

    if (MAP_FAILED != (*thiz)->data) {
        TEMP_FAILURE_RETRY(munmap((*thiz)->data, (*thiz)->size));
    }

    if ((*thiz)->fd >= 0) {
        close((*thiz)->fd);
    }

    shared_library_symbol_t* symbol;
    shared_library_symbol_t* next;

    TAILQ_FOREACH_SAFE(symbol, &(*thiz)->symbols, link, next) {
        TAILQ_REMOVE(&((*thiz)->symbols), symbol, link);
        free(symbol);
    }

    free(*thiz);
    *thiz = NULL;
}

void* shared_library_lookup(shared_library_t* thiz, const char* symbol) {
    shared_library_symbol_t* s;
    ElfW(Sym)* sym;
    size_t str_off;
    const char* str;

    TAILQ_FOREACH(s, &(thiz->symbols), link) {
        for (size_t offset = s->sym_offset; offset < s->sym_offset + s->sym_size; offset += s->sym_entsize) {
            // .symtab / .dynsym
            if (NULL == (sym = shared_library_get(thiz, offset, sizeof(ElfW(Sym))))) {
                break;
            }

            if (SHN_UNDEF == sym->st_shndx) {
                continue;
            }

            // .strtab / .dynstr
            str_off = s->str_offset + sym->st_name;
            if (str_off >= s->str_offset + s->str_size) {
                continue;
            }

            if (NULL == (str = shared_library_get_string(thiz, str_off)) || 0 != strcmp(symbol, str)) {
                continue;
            }

            return (void*) (thiz->address + sym->st_value - thiz->load_bias);
        }
    }

    return NULL;
}

#ifdef __cplusplus
}
#endif

#pragma clang diagnostic pop
