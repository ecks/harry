#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "util.h"
#include "dblist.h"
#include "rfpbuf.h"

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
             enum rfpbuf_source source)
{
    b->base = b->data = base;
    b->allocated = allocated;
    b->source = source;
    b->size = 0;
    b->l2 = b->l3 = b->l4 = b->l7 = NULL;
    list_poison(&b->list_node);
    b->private_p = NULL;
}

/* Initializes 'b' as an empty ofpbuf that contains the 'allocated' bytes of
 * memory starting at 'base'.  'base' should be the first byte of a region
 * obtained from malloc().  It will be freed (with free()) if 'b' is resized or
 * freed. */
void
rfpbuf_use(struct rfpbuf *b, void *base, size_t allocated)
{
    rfpbuf_use__(b, base, allocated, RFPBUF_MALLOC);
}

/* Initializes 'b' as an empty ofpbuf with an initial capacity of 'size'
 * bytes. */
void
rfpbuf_init(struct rfpbuf *b, size_t size)
{
    rfpbuf_use(b, size ? malloc(size) : NULL, size);
}

/* Creates and returns a new ofpbuf with an initial capacity of 'size'
 * bytes. */
struct rfpbuf *
rfpbuf_new(size_t size)
{
    struct rfpbuf *b = malloc(sizeof *b);
    rfpbuf_init(b, size);
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

/* Returns the number of bytes of headroom in 'b', that is, the number of bytes
 * of unused space in ofpbuf 'b' before the data that is in use.  (Most
 * commonly, the data in a ofpbuf is at its beginning, and thus the ofpbuf's
 * headroom is 0.) */
size_t
rfpbuf_headroom(const struct rfpbuf *b)
{
    return (char*)b->data - (char*)b->base;
}

/* Returns the number of bytes that may be appended to the tail end of ofpbuf
 * 'b' before the ofpbuf must be reallocated. */
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


