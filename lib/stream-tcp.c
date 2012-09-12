#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <errno.h>

#include "util.h"
#include "stream-provider.h"
#include "stream-fd.h"

static int
new_tcp_stream(const char *name, int fd, int connect_status,
               const struct sockaddr_in *remote, struct stream **streamp)
{
    struct sockaddr_in local;
    socklen_t local_len = sizeof local;
    int on = 1;
    int retval;

    /* Get the local IP and port information */
    retval = getsockname(fd, (struct sockaddr *)&local, &local_len);
    if (retval) {
        memset(&local, 0, sizeof local);
    }

    retval = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof on);
    if (retval) {
        close(fd);
        return errno;
    }

    retval = new_fd_stream(name, fd, connect_status, streamp);
    if (!retval) {
        struct stream *stream = *streamp;
        stream_set_remote_ip(stream, remote->sin_addr.s_addr);
        stream_set_remote_port(stream, remote->sin_port);
        stream_set_local_ip(stream, local.sin_addr.s_addr);
        stream_set_local_port(stream, local.sin_port);
    }
    return retval;
}

static int
tcp_open(const char *name, char *suffix, struct stream **streamp, uint8_t dscp)
{
    struct sockaddr_in sin;
    int fd, error;

    error = inet_open_active(SOCK_STREAM, suffix, 0, &sin, &fd, dscp);
    if (fd >= 0) {
        return new_tcp_stream(name, fd, error, &sin, streamp);
    } else {
        return error;
    }
}

const struct stream_class tcp_stream_class = {
    "tcp",                      /* name */
    true,                       /* needs_probes */
    tcp_open,                   /* open */
    NULL,                       /* close */
    NULL,                       /* connect */
    NULL,                       /* recv */
    NULL,                       /* send */
    NULL,                       /* run */
    NULL,                       /* run_wait */
    NULL,                       /* wait */
};
