#ifndef STREAM_H
#define STREAM_H

#include "string.h"
#include "stdint.h"

struct stream
{
  struct stream *next;

  /* Remainder is ***private*** to stream
   * direct access is frowned upon!
   * Use the appropriate functions/macros 
   */
  size_t getp; 		/* next get position */
  size_t endp;		/* last valid data position */
  size_t size;		/* size of data segment */
  unsigned char *data; /* data pointer */
};

/* Utility macros. */
#define STREAM_SIZE(S)  ((S)->size)
  /* number of bytes which can still be written */
#define STREAM_WRITEABLE(S) ((S)->size - (S)->endp)
  /* number of bytes still to be read */
#define STREAM_READABLE(S) ((S)->endp - (S)->getp)

/* Does the I/O error indicate that the operation should be retried later? */
#define ERRNO_IO_RETRY(EN) \
        (((EN) == EAGAIN) || ((EN) == EWOULDBLOCK) || ((EN) == EINTR))

/* Stream prototypes. 
 * For stream_{put,get}S, the S suffix mean:
 *
 * c: character (unsigned byte)
 * w: word (two bytes)
 * l: long (two words)
 * q: quad (four words)
 */
extern struct stream *stream_new (size_t);
extern void stream_free (struct stream *);
extern struct stream * stream_copy (struct stream *, struct stream *src);
extern struct stream *stream_dup (struct stream *);
extern size_t stream_resize (struct stream *, size_t);
extern size_t stream_get_getp (struct stream *);
extern size_t stream_get_endp (struct stream *);
extern size_t stream_get_size (struct stream *);

extern void stream_set_getp (struct stream *, size_t);
extern void stream_forward_getp (struct stream *, size_t);
extern void stream_forward_endp (struct stream *, size_t);

/* steam_put: NULL source zeroes out size_t bytes of stream */
extern void stream_put (struct stream *, const void *, size_t);
extern int stream_putc (struct stream *, uint8_t);
extern int stream_putc_at (struct stream *, size_t, uint8_t);
extern int stream_putw (struct stream *, uint16_t);
extern int stream_putw_at (struct stream *, size_t, uint16_t);
extern int stream_putl (struct stream *, uint32_t);
extern int stream_putl_at (struct stream *, size_t, uint32_t);
extern int stream_putq (struct stream *, uint64_t);
extern int stream_putq_at (struct stream *, size_t, uint64_t);

extern ssize_t stream_recvfrom (struct stream *s, int fd, size_t len, 
                                int flags, struct sockaddr *from, 
                                socklen_t *fromlen);

extern void stream_get (void *, struct stream *, size_t);
extern uint8_t stream_getc (struct stream *);
extern uint8_t stream_getc_from (struct stream *, size_t);
extern uint16_t stream_getw (struct stream *);
extern uint16_t stream_peekw (struct stream *);
extern uint16_t stream_getw_from (struct stream *, size_t);
extern uint32_t stream_getl (struct stream *);
extern uint32_t stream_getl_from (struct stream *, size_t);
extern uint64_t stream_getq (struct stream *);
extern uint64_t stream_getq_from (struct stream *, size_t);

extern void stream_reset (struct stream *);

extern int stream_flush (struct stream *, int);
extern int stream_empty (struct stream *); /* is the stream empty? */

#endif


