#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <errno.h>

#include "util.h"
#include "packets.h"
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
new_tcp6_stream(const char *name, int fd, int connect_status,
               const struct sockaddr_in6 *remote, struct stream **streamp)
{
    struct sockaddr_in6 local;
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
        stream_set_remote_ip6(stream, remote->sin6_addr.s6_addr);
        stream_set_remote_port(stream, remote->sin6_port);
        stream_set_local_ip6(stream, local.sin6_addr.s6_addr);
        stream_set_local_port(stream, local.sin6_port);
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

static int ptcp_accept(int fd, const struct sockaddr *sa, size_t sa_len,
                       struct stream **streamp);

static int
ptcp_open(const char *name, char *suffix, struct pstream **pstreamp,
          uint8_t dscp)
{
  char bound_name[128];
  int fd;
  struct sockaddr_in sin;

  fd = inet_open_passive(SOCK_STREAM, suffix, -1, &sin, dscp);
  if (fd < 0) {
      return -fd;
  }

  sprintf(bound_name, "ptcp%"PRIu16":"IP_FMT,
            ntohs(sin.sin_port), IP_ARGS(&sin.sin_addr.s_addr));
  return new_fd_pstream(bound_name, fd, ptcp_accept, NULL, pstreamp);
}

static int
ptcp_accept(int fd, const struct sockaddr *sa, size_t sa_len,
            struct stream **streamp)
{
    const struct sockaddr_in *sin = (const struct sockaddr_in *) sa;
    char name[128];

    if (sa_len == sizeof(struct sockaddr_in) && sin->sin_family == AF_INET) {
        sprintf(name, "tcp:"IP_FMT, IP_ARGS(&sin->sin_addr));
        sprintf(strchr(name, '\0'), ":%"PRIu16, ntohs(sin->sin_port));
    } else {
        strcpy(name, "tcp");
    }
    return new_tcp_stream(name, fd, 0, sin, streamp);
}

const struct pstream_class ptcp_pstream_class = {
    "ptcp",
    true,
    ptcp_open,
    NULL,
    NULL,
    NULL
};


static int ptcp6_accept(int fd, const struct sockaddr *sa, size_t sa_len,
                       struct stream **streamp);

static int
ptcp6_open(const char *name, char *suffix, struct pstream **pstreamp,
          uint8_t dscp)
{
  char bound_name[128];
  int fd;
  struct sockaddr_in6 sin6;

  fd = inet_open_passive6(SOCK_STREAM, suffix, -1, &sin6, dscp);
  if (fd < 0) {
      return -fd;
  }

  sprintf(bound_name, "ptcp%"PRIu16":"IP6_FMT,
            ntohs(sin6.sin6_port), IP6_ARGS(&sin6.sin6_addr.s6_addr));
  return new_fd_pstream(bound_name, fd, ptcp6_accept, NULL, pstreamp);
}

static int
ptcp6_accept(int fd, const struct sockaddr *sa, size_t sa_len,
            struct stream **streamp)
{
  const struct sockaddr_in6 *sin6 = (const struct sockaddr_in6 *) sa;
  char name[128];

  if (sa_len == sizeof(struct sockaddr_in6) && sin6->sin6_family == AF_INET6) 
  {
      sprintf(name, "tcp:"IP6_FMT, IP6_ARGS(&sin6->sin6_addr));
      sprintf(strchr(name, '\0'), ":%"PRIu16, ntohs(sin6->sin6_port));
  } 
  else 
  {
      strcpy(name, "tcp");
  }
  
  return new_tcp6_stream(name, fd, 0, sin6, streamp);
}

const struct pstream_class ptcp6_pstream_class = {
    "ptcp6",
    true,
    ptcp6_open,
    NULL,
    NULL,
    NULL
};
