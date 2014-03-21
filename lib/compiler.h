#ifndef COMPILER_H
#define COMPILER_H

/* Flag manipulation macros */
#define CHECK_FLAG(V,F)      ((V) & (F))
#define SET_FLAG(V, F) (V) |= (F)
#define UNSET_FLAG(V, F) (V) &= ~(F)

#define OVS_UNUSED __attribute__((__unused__))

#define PRINTF_FORMAT(FMT, ARG1) __attribute__((__format__(printf, FMT, ARG1)))
#define MALLOC_LIKE __attribute__((__malloc__))

#endif
