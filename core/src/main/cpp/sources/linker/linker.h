#ifndef LINKER_H
#define LINKER_H

#include <ctype.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct shared_library shared_library_t;

/**
 * Open the specified <code>pathname</code> as <code>shared_library_t</code>
 *
 * @param pathname the file path
 * @return a <code>shared_library_t</code> or <code>NULL</code> if error occurred
 */
shared_library_t* shared_library_open(const char* pathname);

/**
 * Retrieve the pathname of the specified <code>shared_library_t</code>
 * @param thiz a pointer of <code>shared_library_t</code>
 * @param buf buffer
 * @param len size of buffer
 * @return the pathname
 */
const char* shared_library_get_pathname(shared_library_t* thiz, char* buf, size_t len);

uintptr_t shared_library_get_address(shared_library_t* thiz);

/**
 * Lookup the specified <code>symbol</code> from shared library
 *
 * @param thiz a pointer of <code>shared_library_t</code>
 * @param symbol the symbol name
 * @return the address associated with <code>symbol</code>
 */
void* shared_library_lookup(shared_library_t* thiz, const char* symbol);

/**
 * Close the specified <code>shared_library_t</code>
 * @param thiz a pointer of <code>shared_library_t</code>
 */
void shared_library_close(shared_library_t** thiz);

#ifdef __cplusplus
}
#endif

#endif /* LINKER_H */
