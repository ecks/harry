#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <netdb.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "util.h"
#include "routeflow-common.h"
#include "socket-util.h"
#include "sockunion.h"
#include "thread.h"
#include "dblist.h"
#include "rfpbuf.h"
#include "rfp-msgs.h"
#include "ext_client_provider.h"

#define BGP_PORT_DEFAULT 179
#define BGP_HEADER_SIZE 19
#define BGP_MARKER_SIZE 16

enum event {EXT_CLIENT_BGP_ACCEPT, EXT_CLIENT_BGP_READ};

extern struct thread_master * master;

struct ext_client_bgp
{
  struct ext_client ext_client;
  struct list listen_sockets;
};

struct bgp_listener
{
  struct list node;
  unsigned int accept_fd;
  unsigned int peer_fd;
  union sockunion su;
  struct thread * t_accept;
  struct thread * t_read;
};

static void ext_client_bgp_event(enum event event, struct ext_client_bgp * ext_client_bgp, struct bgp_listener * listener);
 
static struct ext_client_bgp * ext_client_bgp_new()
{
  return calloc(1, sizeof(struct ext_client_bgp));
}

struct ext_client * ext_client_bgp_init(char * host, struct punter_ctrl * punter_ctrl)
{
  struct ext_client_bgp * ext_client_bgp = ext_client_bgp_new();

  P(ext_client_bgp).host = host;
  P(ext_client_bgp).punter_ctrl = punter_ctrl;
  P(ext_client_bgp).sockfd = -1;
  P(ext_client_bgp).ibuf = NULL;
  P(ext_client_bgp).obuf = NULL;

  list_init(&ext_client_bgp->listen_sockets);

  ext_client_bgp_socket(ext_client_bgp);

  return &ext_client_bgp->ext_client;
}

static int ext_client_bgp_accept(struct thread * thread)
{
  int accept_fd;
  union sockunion su;
  struct ext_client_bgp * ext_client_bgp = THREAD_ARG(thread);
  struct bgp_listener * listener;

  accept_fd = THREAD_FD(thread);
  if (accept_fd < 0)
  {   
    printf("accept_fd is negative value %d", accept_fd);
    return -1; 
  }   

  LIST_FOR_EACH(listener, struct bgp_listener, node, &ext_client_bgp->listen_sockets)
  {
    // if we found a listener matching fd, we are done
    if(listener->accept_fd == accept_fd)
      break;
  }

  ext_client_bgp_event(EXT_CLIENT_BGP_ACCEPT, ext_client_bgp, listener);

  /* Accept connections */
  listener->peer_fd = sockunion_accept (accept_fd, &su);
  if (listener->peer_fd < 0)
  {   
    printf("[Error] BGP socket accept failed (%s)", strerror (errno));
    return -1; 
  } 

  ext_client_bgp_event(EXT_CLIENT_BGP_READ, ext_client_bgp, listener);
  return 0;
}

static int ext_client_bgp_listener(struct ext_client_bgp * ext_client_bgp, int sock, struct sockaddr * sa, socklen_t salen)
{
  struct bgp_listener *listener;
  int ret, en; 

  sockopt_reuseaddr (sock);
  sockopt_reuseport (sock);

#ifdef IPTOS_PREC_INTERNETCONTROL
  if (sa->sa_family == AF_INET)
    setsockopt_ipv4_tos (sock, IPTOS_PREC_INTERNETCONTROL);
#endif

#ifdef IPV6_V6ONLY
  /* Want only IPV6 on ipv6 socket (not mapped addresses) */
  if (sa->sa_family == AF_INET6) {
    int on = 1;
    setsockopt (sock, IPPROTO_IPV6, IPV6_V6ONLY,
      (void *) &on, sizeof (on));
  }
#endif

  ret = bind (sock, sa, salen);
  if (ret < 0)
  {
    printf ("bind: %s", strerror (en));
    return ret;
  }

  ret = listen (sock, 3);
  if (ret < 0)
  {
    printf("listen: %s", strerror (errno));
    return ret;
  }

  listener = calloc(1, sizeof(struct bgp_listener));
  listener->accept_fd = sock;
  listener->peer_fd = -1;
  memcpy(&listener->su, sa, salen);
  ext_client_bgp_event(EXT_CLIENT_BGP_ACCEPT, ext_client_bgp, listener); 
  list_push_back(&ext_client_bgp->listen_sockets, &listener->node);

  return 0;
}

/* IPv6 supported version of BGP server socket setup. */
int ext_client_bgp_socket(struct ext_client_bgp * ext_client_bgp)
{
  unsigned short port = BGP_PORT_DEFAULT;
  const char * address = NULL;
  struct addrinfo * ainfo;
  struct addrinfo * ainfo_save;
  static const struct addrinfo req = { 
    .ai_family = AF_UNSPEC,
    .ai_flags = AI_PASSIVE,
    .ai_socktype = SOCK_STREAM,
  };  
  int ret, count;
  char port_str[BUFSIZ];

  snprintf (port_str, sizeof(port_str), "%d", port);
  port_str[sizeof (port_str) - 1] = '\0';

  ret = getaddrinfo (address, port_str, &req, &ainfo_save);
  if (ret != 0)
  {   
    printf("getaddrinfo: %s", gai_strerror (ret));
    return -1; 
  }   

  count = 0;
  for (ainfo = ainfo_save; ainfo; ainfo = ainfo->ai_next)
  {   
    int sock;

    if (ainfo->ai_family != AF_INET && ainfo->ai_family != AF_INET6)
      continue;
        
    sock = socket (ainfo->ai_family, ainfo->ai_socktype, ainfo->ai_protocol);
    if (sock < 0)
    {   
      printf("socket: %s", strerror (errno));
      continue;
    }   

    ret = ext_client_bgp_listener (ext_client_bgp, sock, ainfo->ai_addr, ainfo->ai_addrlen);
    if (ret == 0)
      ++count;
    else
      close(sock);
  }
  freeaddrinfo (ainfo_save);
  if (count == 0)
  {
    printf("%s: no usable addresses", __func__);
    return -1;
  }

  return 0;
}
void ext_client_bgp_send(struct ext_client * ext_client)
{

}

static int ext_client_bgp_recv(struct thread * t)
{
  struct ext_client_bgp * ext_client_bgp = THREAD_ARG(t);
  struct bgp_header * bgph;
  u_int16_t length;
  size_t bgph_size = sizeof(struct bgp_header);
  struct rfp_forward_bgp * rfpb;
  size_t already = 0;
  size_t nbytes;
  struct rfpbuf * bgp_buf = rfpbuf_new(bgph_size);
  struct bgp_listener * listener;
  unsigned int peer_fd = THREAD_FD(t);

  printf("ext_client_bgp_recv called\n");

  LIST_FOR_EACH(listener, struct bgp_listener, node, &ext_client_bgp->listen_sockets)
  {
    // if we found a listener matching fd, we are done
    if(listener->peer_fd == peer_fd)
      break;
  }


  if(((nbytes = rfpbuf_read_try(bgp_buf, peer_fd, bgph_size - already)) == 0) ||
      (nbytes == -1))
  {
    // failed
    return -1;
  }
  if(nbytes != bgph_size - already)
  {
    /* Try again later */
    ext_client_bgp_event(EXT_CLIENT_BGP_READ, ext_client_bgp, listener);
    return 0;
  }

  already = bgph_size;

  bgph = rfpbuf_at_assert(bgp_buf, 0, sizeof(struct bgp_header));
  length = ntohs(bgph->length);
  rfpbuf_prealloc_tailroom(bgp_buf, length - already);

  if(already < length)
  {
    if(((nbytes = rfpbuf_read_try(bgp_buf, peer_fd, length - already)) == 0) ||
      (nbytes == -1))
    {
      // failed
      return -1;
    }
    if(nbytes != length - already)
    {
      /* Try again later */
      ext_client_bgp_event(EXT_CLIENT_BGP_READ, ext_client_bgp, listener);
      return 0;
    }
  }

  // put a header
  P(ext_client_bgp).ibuf = routeflow_alloc_xid(RFPT_FORWARD_BGP, RFP10_VERSION, 0, sizeof(struct rfp_forward_bgp));

  rfpb = rfpbuf_at_assert(P(ext_client_bgp).ibuf, 0, sizeof(struct rfp_forward_bgp));

  memcpy(&rfpb->bgp_header, bgp_buf->data, bgph_size);

  // call our parent's class
  ext_client_recv(&P(ext_client_bgp));

  // sent the data so free the memomry
  rfpbuf_delete(bgp_buf);
  rfpbuf_delete(P(ext_client_bgp).ibuf);
  P(ext_client_bgp).ibuf = 0;

  ext_client_bgp_event(EXT_CLIENT_BGP_READ, ext_client_bgp, listener);
  return 0;
}

static void ext_client_bgp_event(enum event event, struct ext_client_bgp * ext_client_bgp, struct bgp_listener * listener)
{
  switch(event)
  { 
    case EXT_CLIENT_BGP_ACCEPT:
      listener->t_accept = thread_add_read(master, ext_client_bgp_accept, ext_client_bgp, listener->accept_fd);
      break;

    case EXT_CLIENT_BGP_READ:
      listener->t_read = thread_add_read(master, ext_client_bgp_recv, ext_client_bgp, listener->peer_fd);
      break;

    default:
      break;
  }

}

#define EXT_CLIENT_INIT                   \
  {                                       \
    ext_client_bgp_init,                  \
    ext_client_bgp_send,                  \
  }

struct ext_client_class bgp_ext_client_class = EXT_CLIENT_INIT;
