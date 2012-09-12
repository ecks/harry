#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "util.h"
#include "stream-fd.h"
#include "stream-provider.h"
#include "stream.h"

/* Active file descriptor stream. */

struct stream_fd
{
    struct stream stream;
    int fd;
};

static const struct stream_class stream_fd_class;

/* Creates a new stream named 'name' that will send and receive data on 'fd'
 * and stores a pointer to the stream in '*streamp'.  Initial connection status
 * 'connect_status' is interpreted as described for stream_init().
 *
 * Returns 0 if successful, otherwise a positive errno value.  (The current
 * implementation never fails.) */
int
new_fd_stream(const char *name, int fd, int connect_status,
              struct stream **streamp)
{
    struct stream_fd *s;

    s = malloc(sizeof *s);
    stream_init(&s->stream, &stream_fd_class, connect_status, name);
    s->fd = fd;
    *streamp = &s->stream;
    return 0;
}

static struct stream_fd *
stream_fd_cast(struct stream *stream)
{
    stream_assert_class(stream, &stream_fd_class);
    return CONTAINER_OF(stream, struct stream_fd, stream);
}

static void
fd_close(struct stream *stream)
{
    struct stream_fd *s = stream_fd_cast(stream);
    close(s->fd);
    free(s);
}

static int
fd_connect(struct stream *stream)
{
    struct stream_fd *s = stream_fd_cast(stream);
    return check_connection_completion(s->fd);
}

static ssize_t
fd_recv(struct stream *stream, void *buffer, size_t n)
{
    struct stream_fd *s = stream_fd_cast(stream);
    ssize_t retval;

    retval = read(s->fd, buffer, n);
    printf("fd_recv: %d\n", retval);
    return retval >= 0 ? retval : -errno;
}

static ssize_t
fd_send(struct stream *stream, const void *buffer, size_t n)
{
    struct stream_fd *s = stream_fd_cast(stream);
    ssize_t retval;

    retval = write(s->fd, buffer, n);
    return (retval > 0 ? retval
            : retval == 0 ? -EAGAIN
            : -errno);
}

static const struct stream_class stream_fd_class = {
    "fd",                       /* name */
    false,                      /* needs_probes */
    NULL,                       /* open */
    fd_close,                   /* close */
    fd_connect,                 /* connect */
    fd_recv,                    /* recv */
    fd_send,                    /* send */
    NULL,                       /* run */
    NULL,                       /* run_wait */
    NULL,                    /* wait */
//    fd_wait,                    /* wait */
};
