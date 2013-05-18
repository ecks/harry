#include "config.h"
#include <errno.h>
#include <string.h>
#include <poll.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "util.h"
#include "socket-util.h"

/* Sets 'fd' to non-blocking mode.  Returns 0 if successful, otherwise a
 *  * positive errno value. */
int
set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags != -1) {
        if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1) {
            return 0;
        } else {
            return errno;
        }
    } else {
        return errno;
    }
}

static int
set_dscp(int fd, uint8_t dscp)
{
    int val;

    if (dscp > 63) {
        return EINVAL;
    }

    val = dscp << 2;
    if (setsockopt(fd, IPPROTO_IP, IP_TOS, &val, sizeof val)) {
        return errno;
    }

    return 0;
}

static int
getsockopt_int(int fd, int level, int option, const char *optname, int *valuep)
{
    socklen_t len;
    int value;
    int error;

    len = sizeof value;
    if (getsockopt(fd, level, option, &value, &len)) {
        error = errno;
    } else if (len != sizeof value) {
        error = EINVAL;
    } else {
        error = 0;
    }

    *valuep = error ? 0 : value;
    return error;
}

/* Returns the error condition associated with socket 'fd' and resets the
 * socket's error status. */
int
get_socket_error(int fd)
{
    int error;

    if (getsockopt_int(fd, SOL_SOCKET, SO_ERROR, "SO_ERROR", &error)) {
        error = errno;
    }
    return error;
}

int
check_connection_completion(int fd)
{
    struct pollfd pfd;
    int retval;

    pfd.fd = fd;
    pfd.events = POLLOUT;
    do {
        retval = poll(&pfd, 1, 0);
    } while (retval < 0 && errno == EINTR);
    if (retval == 1) {
        return get_socket_error(fd);
    } else if (retval < 0) {
        return errno;
    } else {
        return EAGAIN;
    }
}

/* Translates 'host_name', which must be a string representation of an IP
 *  * address, into a numeric IP address in '*addr'.  Returns 0 if successful,
 *   * otherwise a positive errno value. Works with IPv4 only */
int
lookup_ip(const char *host_name, struct in_addr *addr)
{
    if (!inet_pton(AF_INET, host_name, addr)) {
        return ENOENT;
    }
    return 0;
}

/* Translates 'host_name', which must be a string representation of an IP
 *  * address, into a numeric IP address in '*addr'.  Returns 0 if successful,
 *   * otherwise a positive errno value. Works with IPv6 */
int
lookup_ip6(const char *host_name, struct in6_addr *addr)
{
    if (!inet_pton(AF_INET6, host_name, addr)) {
        return ENOENT;
    }
    return 0;
}

/* Parses 'target', which should be a string in the format "<host>[:<port>]".
 * <host> is required.  If 'default_port' is nonzero then <port> is optional
 * and defaults to 'default_port'.
 *
 * On success, returns true and stores the parsed remote address into '*sinp'.
 * On failure, logs an error, stores zeros into '*sinp', and returns false. */
bool
inet_parse_active(const char *target_, uint16_t default_port,
                  struct sockaddr_in *sinp)
{
    char *target = xstrdup(target_);
    char *save_ptr = NULL;
    const char *host_name;
    const char *port_string;
    bool ok = false;

    /* Defaults. */
    sinp->sin_family = AF_INET;
    sinp->sin_port = htons(default_port);

    /* Tokenize. */
    host_name = strtok_r(target, ":", &save_ptr);
    port_string = strtok_r(NULL, ":", &save_ptr);
    if (!host_name) {
        goto exit;
    }

    /* Look up IP, port. */
    if (lookup_ip(host_name, &sinp->sin_addr)) {
        goto exit;
    }
    if (port_string && atoi(port_string)) {
        sinp->sin_port = htons(atoi(port_string));
    } else if (!default_port) {
        goto exit;
    }

    ok = true;

exit:
    if (!ok) {
        memset(sinp, 0, sizeof *sinp);
    }
    free(target);
    return ok;
}

/* Opens a non-blocking IPv4 socket of the specified 'style' and connects to
 * 'target', which should be a string in the format "<host>[:<port>]".  <host>
 * is required.  If 'default_port' is nonzero then <port> is optional and
 * defaults to 'default_port'.
 *
 * 'style' should be SOCK_STREAM (for TCP) or SOCK_DGRAM (for UDP).
 *
 * On success, returns 0 (indicating connection complete) or EAGAIN (indicating
 * connection in progress), in which case the new file descriptor is stored
 * into '*fdp'.  On failure, returns a positive errno value other than EAGAIN
 * and stores -1 into '*fdp'.
 *
 * If 'sinp' is non-null, then on success the target address is stored into
 * '*sinp'.
 *
 * 'dscp' becomes the DSCP bits in the IP headers for the new connection.  It
 * should be in the range [0, 63] and will automatically be shifted to the
 * appropriately place in the IP tos field. */
int
inet_open_active(int style, const char *target, uint16_t default_port,
                 struct sockaddr_in *sinp, int *fdp, uint8_t dscp)
{
    struct sockaddr_in sin;
    int fd = -1;
    int error;

    /* Parse. */
    if (!inet_parse_active(target, default_port, &sin)) {
        error = EAFNOSUPPORT;
        goto exit;
    }

    /* Create non-blocking socket. */
    fd = socket(AF_INET, style, 0);
    if (fd < 0) {
        error = errno;
        goto exit;
    }
//    error = set_nonblocking(fd);
//    if (error) {
//        goto exit;
//    }

    /* The dscp bits must be configured before connect() to ensure that the TOS
     * field is set during the connection establishment.  If set after
     * connect(), the handshake SYN frames will be sent with a TOS of 0. */
//    error = set_dscp(fd, dscp);
//    if (error) {
//        goto exit;
//    }
    // dont set dscp for now

    /* Connect. */
    error = connect(fd, (struct sockaddr *)&sin, sizeof sin) == 0 ? 0 : errno;
    if (error == EINPROGRESS) {
        error = EAGAIN;
    }

exit:
    if (!error || error == EAGAIN) {
        if (sinp) {
            *sinp = sin;
        }
    } else if (fd >= 0) {
        close(fd);
    }
    *fdp = fd;
    return error;
}

bool
inet_parse_active6(const char *target_, int default_port,
                   struct sockaddr_in6 * sin6p /*struct addrinfo * hints, struct addrinfo ** addr*/)
{
  char *target = xstrdup(target_);
  char *string_ptr = target;
  char *save_ptr = NULL;
  const char *host_name;
  const char *port_string;
  bool ok = false;
  int port;
  int status;

  /* Defaults. */
  sin6p->sin6_family = AF_INET6;
  sin6p->sin6_port = htons(default_port);
  sin6p->sin6_scope_id = 4;

//  memset(hints, 0, sizeof(*hints));
//  hints->ai_family = AF_INET6;
//  hints->ai_socktype = SOCK_STREAM;

  /* Tokenize. */
  host_name = strtok_r(target, "-", &save_ptr);
  port_string = strtok_r(NULL, "-", &save_ptr);
  if(!host_name) 
  {
    goto exit;
  }

  /* Look up IP, port. */
  if(lookup_ip6(host_name, &sin6p->sin6_addr))
  { 
    goto exit;
  }

//  if((status = getaddrinfo(host_name, port_string, hints, addr)) != 0)
//  {
//    goto exit;
//  }

  if(port_string && atoi(port_string)) {
    sin6p->sin6_port = htons(atoi(port_string));
  } else if (!default_port) {
    goto exit;
  }

  /* Parse optional bind IP. */
//  host_name = strsep(&string_ptr, ":");
//  if (host_name && host_name[0] && lookup_ip6(host_name, &sin6p->sin_addr)) {
//      goto exit;
//  }


  ok = true;

exit:
  if(!ok)
  {
    memset(sin6p, 0, sizeof *sin6p);
  }
  free(target);
  return ok;
}

int 
inet_open_active6(int style, const char *target, uint16_t default_port,
                  struct sockaddr_in6 *sin6p, int *fdp, uint8_t dscp)
{
//  struct addrinfo hints, *addr;
  struct sockaddr_in6 sin6;
  int fd = -1;
  int error;

  /* Parse. */
  if(!inet_parse_active6(target, default_port, &sin6))
  {
    return -EAFNOSUPPORT;
    goto exit;
  }

  fd = socket(AF_INET6, style, 0);
  if(fd < 0)
  {
    error = errno;
    printf("%s: socket: %s\n", target, strerror(error));
    return -error;
  }
//  error = set_nonblocking(fd);
//  if(error) {
//    goto error;
//  }

  // dont set dscp for now

  /* Connect. */
  error = connect(fd, (struct sockaddr *)&sin6, sizeof sin6) == 0 ? 0 : errno;
  if(error == EINPROGRESS) 
  {
    error = EAGAIN;
  }

exit:
  if(!error || error == EAGAIN)
  {
    if(sin6p)
    {
      *sin6p = sin6;
    }
  }
  else if(fd >= 0)
  {
    close(fd);
  }
  *fdp = fd;
  return error;
}

/* Parses 'target', which should be a string in the format "[<port>][:<ip>]":
 *
 *      - If 'default_port' is -1, then <port> is required.  Otherwise, if
 *        <port> is omitted, then 'default_port' is used instead.
 *
 *      - If <port> (or 'default_port', if used) is 0, then no port is bound
 *        and the TCP/IP stack will select a port.
 *
 *      - If <ip> is omitted then the IP address is wildcarded.
 *
 * If successful, stores the address into '*sinp' and returns true; otherwise
 * zeros '*sinp' and returns false. */
bool
inet_parse_passive(const char *target_, int default_port,
                   struct sockaddr_in *sinp)
{
    char *target = xstrdup(target_);
    char *string_ptr = target;
    const char *host_name;
    const char *port_string;
    bool ok = false;
    int port;

    /* Address defaults. */
    memset(sinp, 0, sizeof *sinp);
    sinp->sin_family = AF_INET;
    sinp->sin_addr.s_addr = htonl(INADDR_ANY);
    sinp->sin_port = htons(default_port);

    /* Parse optional port number. */
    port_string = strsep(&string_ptr, ":");
    if (port_string && str_to_int(port_string, 10, &port)) {
        sinp->sin_port = htons(port);
    } else if (default_port < 0) {
        printf("%s: port number must be specified\n", target_);
        goto exit;
    }

    /* Parse optional bind IP. */
    host_name = strsep(&string_ptr, ":");
    if (host_name && host_name[0] && lookup_ip(host_name, &sinp->sin_addr)) {
        goto exit;
    }

    ok = true;

exit:
    if (!ok) {
        memset(sinp, 0, sizeof *sinp);
    }
    free(target);
    return ok;
}

/* Opens a non-blocking IPv4 socket of the specified 'style', binds to
 * 'target', and listens for incoming connections.  Parses 'target' in the same
 * way was inet_parse_passive().
 *
 * 'style' should be SOCK_STREAM (for TCP) or SOCK_DGRAM (for UDP).
 *
 * For TCP, the socket will have SO_REUSEADDR turned on.
 *
 * On success, returns a non-negative file descriptor.  On failure, returns a
 * negative errno value.
 *
 * If 'sinp' is non-null, then on success the bound address is stored into
 * '*sinp'.
 *
 * 'dscp' becomes the DSCP bits in the IP headers for the new connection.  It
 * should be in the range [0, 63] and will automatically be shifted to the
 * appropriately place in the IP tos field. */
int
inet_open_passive(int style, const char *target, int default_port,
                  struct sockaddr_in *sinp, uint8_t dscp)
{
    struct sockaddr_in sin;
    int fd = 0, error;
    unsigned int yes = 1;

    if (!inet_parse_passive(target, default_port, &sin)) {
        return -EAFNOSUPPORT;
    }

    /* Create non-blocking socket, set SO_REUSEADDR. */
    fd = socket(AF_INET, style, 0);
    if (fd < 0) {
        error = errno;
        printf("%s: socket: %s\n", target, strerror(error));
        return -error;
    }
    error = set_nonblocking(fd);
    if (error) {
        goto error;
    }
    if (style == SOCK_STREAM
        && setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) < 0) {
        error = errno;
        printf("%s: setsockopt(SO_REUSEADDR): %s\n", target, strerror(error));
        goto error;
    }

    /* Bind. */
    if (bind(fd, (struct sockaddr *) &sin, sizeof sin) < 0) {
        error = errno;
        printf("%s: bind: %s\n", target, strerror(error));
        goto error;
    }

    /* The dscp bits must be configured before connect() to ensure that the TOS
     * field is set during the connection establishment.  If set after
     * connect(), the handshake SYN frames will be sent with a TOS of 0. */
//    error = set_dscp(fd, dscp);
//    if (error) {
//        printf("%s: socket: %s\n", target, strerror(error));
//        goto error;
//    }

    /* Listen. */
    if (style == SOCK_STREAM && listen(fd, 10) < 0) {
        error = errno;
        printf("%s: listen: %s\n", target, strerror(error));
        goto error;
    }

    if (sinp) {
        socklen_t sin_len = sizeof sin;
        if (getsockname(fd, (struct sockaddr *) &sin, &sin_len) < 0){
            error = errno;
            printf("%s: getsockname: %s\n", target, strerror(error));
            goto error;
        }
        if (sin.sin_family != AF_INET || sin_len != sizeof sin) {
            error = EAFNOSUPPORT;
            printf("%s: getsockname: invalid socket name\n", target);
            goto error;
        }
        *sinp = sin;
    }

    return fd;

error:
    close(fd);
    return -error;
}


bool
inet_parse_passive6(const char *target_, int default_port,
                   struct sockaddr_in6 *sin6p)
{
  char *target = xstrdup(target_);
  char *string_ptr = target;
  const char *host_name;
  const char *port_string;
  bool ok = false;
  int port;

  /* Address defaults. */
  memset(sin6p, 0, sizeof *sin6p);
  sin6p->sin6_family = AF_INET6;
  sin6p->sin6_addr = in6addr_any;
  sin6p->sin6_port = htons(default_port);

  /* Parse optional port number. */
  port_string = strsep(&string_ptr, ":");
  if (port_string && str_to_int(port_string, 10, &port)) {
      sin6p->sin6_port = htons(port);
  } else if (default_port < 0) {
      printf("%s: port number must be specified\n", target_);
      goto exit;
  }

  /* Parse optional bind IP. */
  host_name = strsep(&string_ptr, ":");
  if (host_name && host_name[0] && lookup_ip6(host_name, &sin6p->sin6_addr)) {
      goto exit;
  }

  ok = true;

exit:
    if (!ok) {
        memset(sin6p, 0, sizeof *sin6p);
    }
    free(target);
    return ok;
}

int
inet_open_passive6(int style, const char *target, int default_port,
                  struct sockaddr_in6 *sin6p, uint8_t dscp)
{
  struct sockaddr_in6 sin6;
  int fd = 0, error;
  unsigned int yes = 1;

  if (!inet_parse_passive6(target, default_port, &sin6)) {
      return -EAFNOSUPPORT;
  }

  /* Create non-blocking socket, set SO_REUSEADDR. */
  fd = socket(AF_INET6, style, 0);
  if (fd < 0) {
      error = errno;
      printf("%s: socket: %s\n", target, strerror(error));
      return -error;
  }
  error = set_nonblocking(fd);
  if (error) {
      goto error;
  }
  if (style == SOCK_STREAM
      && setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) < 0) {
      error = errno;
      printf("%s: setsockopt(SO_REUSEADDR): %s\n", target, strerror(error));
      goto error;
  }

  /* Bind. */
  if (bind(fd, (struct sockaddr *) &sin6, sizeof sin6) < 0) {
      error = errno;
      printf("%s: bind: %s\n", target, strerror(error));
      goto error;
  }

    /* The dscp bits must be configured before connect() to ensure that the TOS
     * field is set during the connection establishment.  If set after
     * connect(), the handshake SYN frames will be sent with a TOS of 0. */
//    error = set_dscp(fd, dscp);
//    if (error) {
//        printf("%s: socket: %s\n", target, strerror(error));
//        goto error;
//    }

  /* Listen. */
  if (style == SOCK_STREAM && listen(fd, 10) < 0) 
  {
    error = errno;
    printf("%s: listen: %s\n", target, strerror(error));
    goto error;
  }

  if (sin6p) {
      socklen_t sin6_len = sizeof sin6;
      if (getsockname(fd, (struct sockaddr *) &sin6, &sin6_len) < 0){
          error = errno;
          printf("%s: getsockname: %s\n", target, strerror(error));
          goto error;
      }
      if (sin6.sin6_family != AF_INET6 || sin6_len != sizeof sin6) {
          error = EAFNOSUPPORT;
          printf("%s: getsockname: invalid socket name\n", target);
          goto error;
      }
      *sin6p = sin6;
  }

  return fd;



error:
    close(fd);
    return -error;
}

struct addrinfo *
host_addrinfo(const char *host, const char *serv, int family, int socktype)
{
        int n;  
        struct addrinfo hints, *res;

        bzero(&hints, sizeof(struct addrinfo));
        hints.ai_flags = AI_CANONNAME;  /* always return canonical name */
        hints.ai_family = family;               /* 0, AF_INET, AF_INET6, etc. */
        hints.ai_socktype = socktype;   /* 0, SOCK_STREAM, SOCK_DGRAM, etc. */

        if ( (n = getaddrinfo(host, serv, &hints, &res)) != 0)
                printf("host_serv error for %s, %s\n",
                                 (host == NULL) ? "(no hostname)" : host,
                                 (serv == NULL) ? "(no service name)" : serv);

        return(res);    /* return pointer to first on linked list */
}

static int
setsockopt_ipv4_ifindex (int sock, int val)
{
  int ret;

#if defined (IP_PKTINFO)
  if ((ret = setsockopt (sock, IPPROTO_IP, IP_PKTINFO, &val, sizeof (val))) < 0)
    fprintf(stderr, "Can't set IP_PKTINFO option for fd %d to %d: %s",
               sock,val,strerror(errno));
#elif defined (IP_RECVIF)
  if ((ret = setsockopt (sock, IPPROTO_IP, IP_RECVIF, &val, sizeof (val))) < 0)
    zlog_warn ("Can't set IP_RECVIF option for fd %d to %d: %s",
               sock,val,safe_strerror(errno));
#else
#warning "Neither IP_PKTINFO nor IP_RECVIF is available."
#warning "Will not be able to receive link info."
#warning "Things might be seriously broken.."
  ret = -1;
#endif
  return ret;
}

#ifdef HAVE_IPV6
/* Set IPv6 packet info to the socket. */
static int
setsockopt_ipv6_pktinfo (int sock, int val)
{
  int ret;

#ifdef IPV6_RECVPKTINFO         /*2292bis-01*/
  ret = setsockopt(sock, IPPROTO_IPV6, IPV6_RECVPKTINFO, &val, sizeof(val));
  if (ret < 0)
    fprintf (stderr, "can't setsockopt IPV6_RECVPKTINFO : %s", strerror (errno));
#else   /*RFC2292*/
  ret = setsockopt(sock, IPPROTO_IPV6, IPV6_PKTINFO, &val, sizeof(val));
  if (ret < 0)
    fprintf (stderr, "can't setsockopt IPV6_PKTINFO : %s", strerror (errno));
#endif /* INIA_IPV6 */
  return ret;
}
#endif

int
setsockopt_ipv4_tos(int sock, int tos)
{
  int ret;

  ret = setsockopt (sock, IPPROTO_IP, IP_TOS, &tos, sizeof (tos));
  if (ret < 0)
    printf("Can't set IP_TOS option for fd %d to %#x: %s",
                                 sock, tos, strerror(errno));
  return ret;
}

int
setsockopt_ifindex (int af, int sock, int val)
{
  int ret = -1; 
  
  switch (af)
    {   
      case AF_INET:
        ret = setsockopt_ipv4_ifindex (sock, val);
        break;
#ifdef HAVE_IPV6
      case AF_INET6:
        ret = setsockopt_ipv6_pktinfo (sock, val);
        break;
#endif
      default:
        fprintf (stderr, "setsockopt_ifindex: unknown address family %d", af);
    }   
  return ret;
}

int
sockopt_reuseaddr (int sock)
{
  int ret;
  int on = 1;

  ret = setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, 
                    (void *) &on, sizeof (on));
  if (ret < 0)
    {   
      fprintf (stderr, "can't set sockopt SO_REUSEADDR to socket %d", sock);
      return -1; 
    }   
  return 0;
}

#ifdef SO_REUSEPORT
int
sockopt_reuseport (int sock)
{
  int ret;
  int on = 1;

  ret = setsockopt (sock, SOL_SOCKET, SO_REUSEPORT, 
                                (void *) &on, sizeof (on));
  if (ret < 0)
  {   
    printf("can't set sockopt SO_REUSEPORT to socket %d", sock);
    return -1; 
  }   
  return 0;
}
#else
int
sockopt_reuseport (int sock)
{
  return 0;
}
#endif /* 0 */
