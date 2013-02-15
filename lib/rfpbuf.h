#ifndef RFPBUF_H
#define RFPBUF_H

typedef enum  
{
    RFPBUF_MALLOC,              /* Obtained via malloc(). */
    RFPBUF_STACK,               /* Un-movable stack space or static buffer. */
    RFPBUF_STUB                 /* Starts on stack, may expand into heap. */
} rfpbuf_source;

typedef enum 
{
  RFPBUF_ERROR = -1,
  RFPBUF_EMPTY = 0,
  RFPBUF_PENDING = 1
} rfpbuf_status;

/* Buffer for holding arbitrary data.  An ofpbuf is automatically reallocated
 * as necessary if it grows too large for the available memory. */
struct rfpbuf {
    void *base;                 /* First byte of allocated space. */
    size_t allocated;           /* Number of bytes allocated. */
    rfpbuf_source source;  /* Source of memory allocated as 'base'. */

    void *data;                 /* First byte actually in use. */
    size_t size;                /* Number of bytes in use. */

    void *l2;                   /* Link-level header. */
    void *l3;                   /* Network-level header. */
    void *l4;                   /* Transport-level header. */
    void *l7;                   /* Application data. */

    struct list list_node;      /* Private list element for use by owner. */
    void *private_p;            /* Private pointer for use by owner. */
};

void rfpbuf_use(struct rfpbuf *, void *, size_t);
void *rfpbuf_tail(const struct rfpbuf *b);
void *rfpbuf_end(const struct rfpbuf *b);
void *rfpbuf_put_uninit(struct rfpbuf *b, size_t size);
void rfpbuf_uninit(struct rfpbuf *);
void rfpbuf_init(struct rfpbuf *, size_t);
void rfpbuf_prealloc_tailroom(struct rfpbuf *, size_t);

struct rfpbuf *rfpbuf_new(size_t);
void rfpbuf_delete(struct rfpbuf *);

rfpbuf_status rfpbuf_write(struct rfpbuf *, int);
ssize_t rfpbuf_read_try(struct rfpbuf * b, int fd, size_t size);
ssize_t rfpbuf_recvmsg(struct rfpbuf * b, int fd, struct msghdr * msgh, int flags, size_t size);

void *rfpbuf_at_assert(const struct rfpbuf *, size_t offset, size_t size);

#endif
