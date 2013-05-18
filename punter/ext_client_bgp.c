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

#include "routeflow-common.h"
#include "socket-util.h"
#include "thread.h"
#include "ext_client_provider.h"

#define BGP_PORT_DEFAULT 179

extern struct thread_master * master;

struct ext_client_bgp
{
  struct ext_client ext_client;
//  struct sockunion su; // figure out whether sockunion here is important or not
  struct sockaddr su; 
  struct thread * thread;
};
 
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


  bgp_socket(ext_client_bgp);

  return &ext_client_bgp->ext_client;
}

static int bgp_accept(struct thread * thread)
{
  printf("called bgp_accept!\n");
  return 0;
}

static int bgp_listener(struct ext_client_bgp * ext_client_bgp, int sock, struct sockaddr * sa, socklen_t salen)
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

  P(ext_client_bgp).sockfd = sock;
  memcpy(&ext_client_bgp->su, sa, salen);
  ext_client_bgp->thread = thread_add_read(master, bgp_accept, ext_client_bgp, sock); 
  return 0;
}

/* IPv6 supported version of BGP server socket setup. */
int bgp_socket(struct ext_client_bgp * ext_client_bgp)
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

    ret = bgp_listener (ext_client_bgp, sock, ainfo->ai_addr, ainfo->ai_addrlen);
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

#define EXT_CLIENT_INIT                   \
  {                                       \
    ext_client_bgp_init,                  \
    ext_client_bgp_send,                  \
  }

struct ext_client_class bgp_ext_client_class = EXT_CLIENT_INIT;
