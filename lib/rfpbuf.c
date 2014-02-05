#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>

#include "util.h"
#include "dblist.h"
#include "rfpbuf.h"

size_t rfpbuf_tailroom(const struct rfpbuf *b);

/* Returns a pointer to byte 'offset' in 'b', which must contain at least
 * 'offset + size' bytes of data. */
void *
rfpbuf_at_assert(const struct rfpbuf *b, size_t offset, size_t size)
{
  assert(offset + size <= b->size);

  return ((char *) b->data) + offset;
}

static void
rfpbuf_use__(struct rfpbuf *b, void *base, size_t allocated,
             rfpbuf_source source)
{
    b->base = b->data = base;
    b->allocated = allocated;
    b->source = source;
    b->size = 0;
    b->l2 = b->l3 = b->l4 = b->l7 = NULL;
    list_poison(&b->list_node);
    b->private_p = NULL;
}

/* Initializes 'b' as an empty rfpbuf that contains the 'allocated' bytes of
 * memory starting at 'base'.  'base' should be the first byte of a region
 * obtained from malloc().  It will be freed (with free()) if 'b' is resized or
 * freed. */
void
rfpbuf_use(struct rfpbuf *b, void *base, size_t allocated)
{
    rfpbuf_use__(b, base, allocated, RFPBUF_MALLOC);
}

/* Initializes 'b' as an empty rfpbuf with an initial capacity of 'size'
 * bytes. */
void
rfpbuf_init(struct rfpbuf *b, size_t size)
{
    rfpbuf_use(b, size ? malloc(size) : NULL, size);
}

/* Creates and returns a new rfpbuf with an initial capacity of 'size'
 * bytes. */
struct rfpbuf *
rfpbuf_new(size_t size)
{
    struct rfpbuf *b = malloc(sizeof *b);
    rfpbuf_init(b, size);
    return b;
}

/* Creates and returns a new rfpbuf with an initial capacity of 'size +
 * headroom' bytes, reserving the first 'headroom' bytes as headroom. */
struct rfpbuf *
rfpbuf_new_with_headroom(size_t size, size_t headroom)
{
    struct rfpbuf *b = rfpbuf_new(size + headroom);
    rfpbuf_reserve(b, headroom);
    return b;
}

/* Creates and returns a new rfpbuf that initially contains a copy of the
 * 'buffer->size' bytes of data starting at 'buffer->data' with no headroom or
 * tailroom. */
struct rfpbuf *
rfpbuf_clone(const struct rfpbuf *buffer)
{
    return rfpbuf_clone_with_headroom(buffer, 0);
}

/* Creates and returns a new rfpbuf whose data are copied from 'buffer'.   The
 * returned rfpbuf will additionally have 'headroom' bytes of headroom. */
struct rfpbuf *
rfpbuf_clone_with_headroom(const struct rfpbuf *buffer, size_t headroom)
{
    struct rfpbuf *new_buffer;
    uintptr_t data_delta;

    new_buffer = rfpbuf_clone_data_with_headroom(buffer->data, buffer->size,
                                                 headroom);
    data_delta = (char *) new_buffer->data - (char *) buffer->data;

    if (buffer->l2) {
        new_buffer->l2 = (char *) buffer->l2 + data_delta;
    }
    if (buffer->l3) {
        new_buffer->l3 = (char *) buffer->l3 + data_delta;
    }
    if (buffer->l4) {
        new_buffer->l4 = (char *) buffer->l4 + data_delta;
    }
    if (buffer->l7) {
        new_buffer->l7 = (char *) buffer->l7 + data_delta;
    }

    return new_buffer;
}

/* Creates and returns a new rfpbuf that initially contains a copy of the
 * 'size' bytes of data starting at 'data' with no headroom or tailroom. */
struct rfpbuf *
rfpbuf_clone_data(const void *data, size_t size)
{
    return rfpbuf_clone_data_with_headroom(data, size, 0);
}

/* Creates and returns a new rfpbuf that initially contains 'headroom' bytes of
 * headroom followed by a copy of the 'size' bytes of data starting at
 * 'data'. */
struct rfpbuf *
rfpbuf_clone_data_with_headroom(const void *data, size_t size, size_t headroom)
{
    struct rfpbuf *b = rfpbuf_new_with_headroom(size, headroom);
    rfpbuf_put(b, data, size);
    return b;
}

/* Returns the byte following the last byte of data in use in 'b'. */
void *
rfpbuf_tail(const struct rfpbuf *b)
{
    return (char *) b->data + b->size;
}

/* Returns the byte following the last byte allocated for use (but not
 * necessarily in use) by 'b'. */
void *
rfpbuf_end(const struct rfpbuf *b)
{
    return (char *) b->base + b->allocated;
}

/* Appends 'size' bytes of data to the tail end of 'b', reallocating and
 * copying its data if necessary.  Returns a pointer to the first byte of the
 * new data, which is left uninitialized. */
void *
rfpbuf_put_uninit(struct rfpbuf *b, size_t size)
{
    void *p;
    rfpbuf_prealloc_tailroom(b, size);
    p = rfpbuf_tail(b);
    b->size += size;
    return p;
}

/* Appends data to end of tail
 */
void * 
rfpbuf_put(struct rfpbuf *b, const void * p, size_t size)
{
  void * dst = rfpbuf_put_uninit(b, size);
  memcpy(dst, p, size);
  return dst;
}

/* Reserves 'size' bytes of headroom so that they can be later allocated with
 * rfpbuf_push_uninit() without reallocating the rfpbuf. */
void
rfpbuf_reserve(struct rfpbuf *b, size_t size)
{
    rfpbuf_prealloc_tailroom(b, size);
    b->data = (char*)b->data + size;
}

/* Frees memory that 'b' points to. */
void
rfpbuf_uninit(struct rfpbuf *b)
{
    if (b && b->source == RFPBUF_MALLOC) {
        free(b->base);
    }
}

/* Frees memory that 'b' points to, as well as 'b' itself. */
void
rfpbuf_delete(struct rfpbuf *b)
{
    if (b) {
        rfpbuf_uninit(b);
        free(b);
    }
}

rfpbuf_status rfpbuf_write(struct rfpbuf * b, int sock_fd)
{
  size_t nbytes;

  if((nbytes = write(sock_fd, rfpbuf_at_assert(b, 0, b->size), b->size)) < 0)
  {
    perror("Error on write");
    return RFPBUF_ERROR;
  }

  if(nbytes < b->size)
    return RFPBUF_PENDING;
  else
    return RFPBUF_EMPTY;
}

ssize_t rfpbuf_read_try(struct rfpbuf * b, int fd, size_t size)
{
  ssize_t nbytes;
  void * p;

  p = rfpbuf_tail(b);

  if((nbytes = read(fd, p, size)) >= 0)
  {
    b->size += nbytes;
    return nbytes;
  }

  return -1;
}

ssize_t rfpbuf_recvmsg(struct rfpbuf * b, int fd, struct msghdr * msgh, int flags, 
                       size_t size)
{
  int nbytes;
  struct iovec * iov;

  assert(msgh->msg_iovlen > 0);
  
  if(rfpbuf_tailroom(b) < size)
  {
    fprintf(stderr, "Not enough room");
    return -1;
  }

  iov = &(msgh->msg_iov[0]);
  iov->iov_base = rfpbuf_tail(b);
  iov->iov_len = size;

  nbytes = recvmsg(fd, msgh, flags);

  if(nbytes > 0)
  {
    b->size =+ nbytes;
  }

  printf("%d bytes received!\n", nbytes);

  return nbytes;
}

/* Returns the number of bytes of headroom in 'b', that is, the number of bytes
 * of unused space in rfpbuf 'b' before the data that is in use.  (Most
 * commonly, the data in a rfpbuf is at its beginning, and thus the rfpbuf's
 * headroom is 0.) */
size_t
rfpbuf_headroom(const struct rfpbuf *b)
{
    return (char*)b->data - (char*)b->base;
}

/* Returns the number of bytes that may be appended to the tail end of rfpbuf
 * 'b' before the rfpbuf must be reallocated. */
size_t
rfpbuf_tailroom(const struct rfpbuf *b)
{
    return (char*)rfpbuf_end(b) - (char*)rfpbuf_tail(b);
}

static void
rfpbuf_copy__(struct rfpbuf *b, uint8_t *new_base,
              size_t new_headroom, size_t new_tailroom)
{
    const uint8_t *old_base = b->base;
    size_t old_headroom = rfpbuf_headroom(b);
    size_t old_tailroom = rfpbuf_tailroom(b);
    size_t copy_headroom = MIN(old_headroom, new_headroom);
    size_t copy_tailroom = MIN(old_tailroom, new_tailroom);

    memcpy(&new_base[new_headroom - copy_headroom],
           &old_base[old_headroom - copy_headroom],
           copy_headroom + b->size + copy_tailroom);
}

/* Reallocates 'b' so that it has exactly 'new_headroom' and 'new_tailroom'
 * bytes of headroom and tailroom, respectively. */
static void
rfpbuf_resize__(struct rfpbuf *b, size_t new_headroom, size_t new_tailroom)
{
    void *new_base, *new_data;
    size_t new_allocated;

    new_allocated = new_headroom + b->size + new_tailroom;

    switch (b->source) {
    case RFPBUF_MALLOC:
        if (new_headroom == rfpbuf_headroom(b)) {
            new_base = realloc(b->base, new_allocated);
        } else {
            new_base = malloc(new_allocated);
            rfpbuf_copy__(b, new_base, new_headroom, new_tailroom);
            free(b->base);
        }
        break;

    case RFPBUF_STACK:
        NOT_REACHED();

    case RFPBUF_STUB:
        b->source = RFPBUF_MALLOC;
        new_base = malloc(new_allocated);
        rfpbuf_copy__(b, new_base, new_headroom, new_tailroom);
        break;

    default:
        NOT_REACHED();
    }

    b->allocated = new_allocated;
    b->base = new_base;

    new_data = (char *) new_base + new_headroom;
    if (b->data != new_data) {
        uintptr_t data_delta = (char *) new_data - (char *) b->data;
        b->data = new_data;
        if (b->l2) {
            b->l2 = (char *) b->l2 + data_delta;
        }
        if (b->l3) {
            b->l3 = (char *) b->l3 + data_delta;
        }
        if (b->l4) {
            b->l4 = (char *) b->l4 + data_delta;
        }
        if (b->l7) {
            b->l7 = (char *) b->l7 + data_delta;
        }
    }
}

/* Ensures that 'b' has room for at least 'size' bytes at its tail end,
 * reallocating and copying its data if necessary.  Its headroom, if any, is
 * preserved. */
void
rfpbuf_prealloc_tailroom(struct rfpbuf *b, size_t size)
{
    if (size > rfpbuf_tailroom(b)) {
        rfpbuf_resize__(b, rfpbuf_headroom(b), MAX(size, 64));
    }
}


