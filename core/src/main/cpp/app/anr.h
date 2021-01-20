#ifndef ANR_H
#define ANR_H

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

int anr_watch(JavaVM* vm);

#ifdef __cplusplus
}
#endif

#endif /* APP_H */
