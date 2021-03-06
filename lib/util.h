#ifndef UTIL_H
#define UTIL_H

#include "compiler.h"

#include <sys/time.h>
#include <stddef.h>

/* Flag manipulation macros. */
#define CHECK_FLAG(V,F)      ((V) & (F))
#define SET_FLAG(V,F)        (V) |= (F)
#define UNSET_FLAG(V,F)      (V) &= ~(F)

/* Given POINTER, the address of the given MEMBER in a STRUCT object, returns
 *    the STRUCT object. */
#define CONTAINER_OF(POINTER, STRUCT, MEMBER)                           \
        ((STRUCT *) (void *) ((char *) (POINTER) - offsetof (STRUCT, MEMBER)))

/* Returns the number of elements in ARRAY. */
#define ARRAY_SIZE(ARRAY) (sizeof ARRAY / sizeof *ARRAY)

#ifndef MIN
#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))
#endif

#ifndef MAX
#define MAX(X, Y) ((X) > (Y) ? (X) : (Y))
#endif

#define NOT_REACHED() abort()

#define CHECK_FLAG(V,F)      ((V) & (F)) 
#define SET_FLAG(V,F)        (V) |= (F)
#define UNSET_FLAG(V,F)      (V) &= ~(F)

typedef struct timeval timestamp_t;

char *xasprintf(const char *format, ...) PRINTF_FORMAT(1, 2) MALLOC_LIKE;
char *xstrdup(const char *) MALLOC_LIKE;

bool str_to_int(const char *, int base, int *);
bool str_to_llong(const char *, int base, long long *);

#endif
