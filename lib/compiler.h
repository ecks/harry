#ifndef COMPILER_H
#define COMPILER_H

#define OVS_UNUSED __attribute__((__unused__))

#define PRINTF_FORMAT(FMT, ARG1) __attribute__((__format__(printf, FMT, ARG1)))
#define MALLOC_LIKE __attribute__((__malloc__))

#endif
