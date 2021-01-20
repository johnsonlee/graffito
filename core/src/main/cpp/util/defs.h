#ifndef DEFS_H
#define DEFS_H

#define UNUSED(x) ((void) x)

#define VOID(...) UNUSED(0)

#define STRINGIFY(x) STRINGIFY2(x)
#define STRINGIFY2(x) #x

#endif /* DEFS_H */
