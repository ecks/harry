#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <poll.h>

#include "util.h"
#include "thread.h"
#include "stream-fd.h"
#include "stream-provider.h"
#include "stream.h"

extern struct thread_master * master;

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
    if(retval < 0)
    {
      perror("read error");
    }
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

static void
fd_wait(struct stream *stream, enum stream_wait_type wait, int (*func)(struct thread *), void * args)
{
    struct stream_fd *s = stream_fd_cast(stream);
    switch (wait) {
    case STREAM_CONNECT:
    case STREAM_SEND:
        poll_fd_wait(s->fd, POLLOUT);
        break;

    case STREAM_RECV:
        printf("fd is %d\n", s->fd);
        thread_add_read(master, func, args, s->fd);
//        poll_fd_wait(s->fd, POLLIN);
        break;

    default:
        NOT_REACHED();
    }
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
    fd_wait,                    /* wait */
};

struct fd_pstream
{
    struct pstream pstream;
    int fd;
    int (*accept_cb)(int fd, const struct sockaddr *, size_t sa_len,
                     struct stream **);
    char *unlink_path;
};

static struct pstream_class fd_pstream_class;

static struct fd_pstream *
fd_pstream_cast(struct pstream *pstream)
{
    pstream_assert_class(pstream, &fd_pstream_class);
    return CONTAINER_OF(pstream, struct fd_pstream, pstream);
}

/* Creates a new pstream named 'name' that will accept new socket connections
 * on 'fd' and stores a pointer to the stream in '*pstreamp'.
 *
 * When a connection has been accepted, 'accept_cb' will be called with the new
 * socket fd 'fd' and the remote address of the connection 'sa' and 'sa_len'.
 * accept_cb must return 0 if the connection is successful, in which case it
 * must initialize '*streamp' to the new stream, or a positive errno value on
 * error.  In either case accept_cb takes ownership of the 'fd' passed in.
 *
 * When '*pstreamp' is closed, then 'unlink_path' (if nonnull) will be passed
 * to fatal_signal_unlink_file_now() and freed with free().
 *
 * Returns 0 if successful, otherwise a positive errno value.  (The current
 * implementation never fails.) */
int
new_fd_pstream(const char *name, int fd,
               int (*accept_cb)(int fd, const struct sockaddr *sa,
                                size_t sa_len, struct stream **streamp),
               char *unlink_path, struct pstream **pstreamp)
{
    struct fd_pstream *ps = malloc(sizeof *ps);
    pstream_init(&ps->pstream, &fd_pstream_class, name);
    ps->fd = fd;
    ps->accept_cb = accept_cb;
    ps->unlink_path = unlink_path;
    *pstreamp = &ps->pstream;
    return 0;
}

static void
pfd_close(struct pstream *pstream)
{

}

static int
pfd_accept(struct pstream *pstream, struct stream **new_streamp)
{
    struct fd_pstream *ps = fd_pstream_cast(pstream);
    struct sockaddr_storage ss;
    socklen_t ss_len = sizeof ss;
    int new_fd;
    int retval;

    new_fd = accept(ps->fd, (struct sockaddr *) &ss, &ss_len);
    if (new_fd < 0) {
        retval = errno;
        printf("accept: %s\n", strerror(retval));
        if (retval != EAGAIN) {
            printf("accept: %s\n", strerror(retval));
        }
        return retval;
    }

    retval = set_nonblocking(new_fd);
    if (retval) {
        close(new_fd);
        return retval;
    }

    return ps->accept_cb(new_fd, (const struct sockaddr *) &ss, ss_len,
                         new_streamp);
}

static void
pfd_wait(struct pstream *pstream, int (*func)(struct thread *), void * args)
{
    struct fd_pstream *ps = fd_pstream_cast(pstream);
//    poll_fd_wait(ps->fd, POLLIN);
    thread_add_read(master, func, args, ps->fd);
}

static struct pstream_class fd_pstream_class = {
    "pstream",
    false,
    NULL,
    pfd_close,
    pfd_accept,
    pfd_wait
};
