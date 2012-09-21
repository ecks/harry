#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <errno.h>

#include "util.h"

void
out_of_memory(void)
{
//    ovs_abort(0, "virtual memory exhausted");
}

void *
xmalloc(size_t size)
{
    void *p = malloc(size ? size : 1);
    if (p == NULL) {
        out_of_memory();
    }
    return p;
}

char *
xmemdup0(const char *p_, size_t length)
{
    char *p = xmalloc(length + 1);
    memcpy(p, p_, length);
    p[length] = '\0';
    return p;
}

char *
xstrdup(const char *s)
{
    return xmemdup0(s, strlen(s));
}

char *
xvasprintf(const char *format, va_list args)
{
    va_list args2;
    size_t needed;
    char *s;

    va_copy(args2, args);
    needed = vsnprintf(NULL, 0, format, args);

    s = xmalloc(needed + 1);

    vsnprintf(s, needed + 1, format, args2);
    va_end(args2);

    return s;
}

char *
xasprintf(const char *format, ...)
{
    va_list args;
    char *s;

    va_start(args, format);
    s = xvasprintf(format, args);
    va_end(args);

    return s;
}

bool
str_to_int(const char *s, int base, int *i)
{
    long long ll;
    bool ok = str_to_llong(s, base, &ll);
    *i = ll;
    return ok;
}

bool
str_to_llong(const char *s, int base, long long *x)
{
    int save_errno = errno;
    char *tail;
    errno = 0;
    *x = strtoll(s, &tail, base);
    if (errno == EINVAL || errno == ERANGE || tail == s || *tail != '\0') {
        errno = save_errno;
        *x = 0;
        return false;
    } else {
        errno = save_errno;
        return true;
    }
}
