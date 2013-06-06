#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "routeflow-common.h"
#include "socket-util.h"
#include "dblist.h"
#include "thread.h"
#include "rfpbuf.h"
#include "rfp-msgs.h"
#include "bgp_ctrl_client.h"

enum bgp_ctrl_client_state
{
  BGP_CTRL_CONNECTING,
  BGP_CTRL_SEND_HELLO,
  BGP_CTRL_RECV_HELLO,
  BGP_CTRL_CONNECTED
};

enum event {BGP_CTRL_CLIENT_SCHEDULE, BGP_CTRL_CLIENT_CONNECT, BGP_CTRL_CLIENT_READ, BGP_CTRL_CLIENT_CONNECTED};

/* Prototype for event manager */
static void bgp_ctrl_client_event(enum event, struct bgp_ctrl_client * bgp_ctrl_client);

extern struct thread_master * master;

struct bgp_ctrl_client * bgp_ctrl_client_new()
{
  return calloc(1, sizeof(struct bgp_ctrl_client));
}

void bgp_ctrl_client_init(struct bgp_ctrl_client * bgp_ctrl_client, struct in6_addr * ctrl_addr)
{
  bgp_ctrl_client->sock = -1;

  bgp_ctrl_client->ctrl_addr = ctrl_addr;

  bgp_ctrl_client->state = BGP_CTRL_CONNECTING;

  bgp_ctrl_client_event(BGP_CTRL_CLIENT_SCHEDULE, bgp_ctrl_client);
}

int bgp_ctrl_client_socket(struct in6_addr * ctrl_addr)
{
  char c_addr[INET6_ADDRSTRLEN+1];
  int sock;
  int ret;
  int status;

  inet_ntop(AF_INET6, ctrl_addr, c_addr, INET6_ADDRSTRLEN+1);

  // Set up socket address info
  struct addrinfo hints, *addr;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET6;     // IPv6
  hints.ai_socktype = SOCK_STREAM;  // TCP
  
  char port_str[8];
  sprintf(port_str, "%u", BGP_CTRL_SISIS_PORT);
  if((status = getaddrinfo(c_addr, port_str, &hints, &addr)) != 0)
  {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    exit(1);
  }
  
  sock = socket (addr->ai_family, addr->ai_socktype, addr->ai_protocol);
  if (sock < 0)
  {
    printf("Network: can't create socket.\n");
  }
           
  printf("bgp_ctrl_client TCP socket: creating socket %d\n", sock);
  
  /* Connect to controller. */
  ret = connect (sock, addr->ai_addr, addr->ai_addrlen);
  if (ret < 0)
  {
    close (sock);
    perror("connect");
    return -1;
  }
 
  return sock;
}

int bgp_ctrl_send_message(struct bgp_ctrl_client * bgp_ctrl_client)
{
  if(bgp_ctrl_client->sock < 0)
  {
    return -1;
  }
  switch(rfpbuf_write(bgp_ctrl_client->obuf, bgp_ctrl_client->sock))
  {
    case RFPBUF_ERROR:
      break;
      
    case RFPBUF_EMPTY:
      break;
  }

  rfpbuf_delete(bgp_ctrl_client->obuf);
  bgp_ctrl_client->obuf = NULL;

  return 0;
}
static int bgp_ctrl_message_send(struct bgp_ctrl_client * bgp_ctrl_client, uint8_t type, uint8_t version)
{
  bgp_ctrl_client->obuf = routeflow_alloc(type, version, sizeof(struct rfp_header));

  return bgp_ctrl_send_message(bgp_ctrl_client);
}

/* connection to controller daemon */
int bgp_ctrl_client_start(struct bgp_ctrl_client * bgp_ctrl_client)
{
  int retval;

  if(bgp_ctrl_client->sock >= 0)
    return 0;

  if(bgp_ctrl_client->t_connect)
    return 0;

  /* Make socket */
  bgp_ctrl_client->sock = bgp_ctrl_client_socket (bgp_ctrl_client->ctrl_addr);
  if (bgp_ctrl_client->sock < 0)
  {
    printf("bgp_ctrl_client connection fail\n");
    bgp_ctrl_client->fail++;
    bgp_ctrl_client_event (BGP_CTRL_CLIENT_CONNECT, bgp_ctrl_client);
    return -1;
  }

  if(set_nonblocking(bgp_ctrl_client->sock) < 0)
  {
    printf("set_nonblocking failed\n");
  }

  /* Clear fail count */
  bgp_ctrl_client->fail = 0;

  bgp_ctrl_client_event(BGP_CTRL_CLIENT_READ, bgp_ctrl_client);

  retval = bgp_ctrl_message_send(bgp_ctrl_client, RFPT_HELLO, RFP10_VERSION);
  if(!retval)
    bgp_ctrl_client->state = BGP_CTRL_RECV_HELLO;

  return 0;
}

/* wrapper function for threads */
static int bgp_ctrl_client_connect(struct thread * t)
{
  struct bgp_ctrl_client * bgp_ctrl_client;

  bgp_ctrl_client = THREAD_ARG(t);
  bgp_ctrl_client->t_connect = NULL;

  return bgp_ctrl_client_start(bgp_ctrl_client);
}

static int bgp_ctrl_client_read(struct thread * t)
{
  int ret;
  size_t already = 0;
  struct rfp_header * rh;
  uint16_t length;
  uint8_t type;
  size_t rh_size = sizeof(struct rfp_header);
  struct bgp_ctrl_client * bgp_ctrl_client = THREAD_ARG(t);
  bgp_ctrl_client->t_read = NULL;

  if(bgp_ctrl_client->ibuf == NULL)
  {
    bgp_ctrl_client->ibuf = rfpbuf_new(rh_size);
  }
  else
  {
    rfpbuf_put_uninit(bgp_ctrl_client->ibuf, rh_size);
  }

  ssize_t nbyte;
  if(((nbyte = rfpbuf_read_try(bgp_ctrl_client->ibuf, bgp_ctrl_client->sock, sizeof(struct rfp_header)-already)) == 0) ||
     (nbyte == -1))
  {
//    return ctrl_client_failed(bgp_ctrl_client);
  }
  if(nbyte != (ssize_t)(sizeof(struct rfp_header)-already))
  {
    /* Try again later */
    bgp_ctrl_client_event(BGP_CTRL_CLIENT_READ, bgp_ctrl_client);
    return 0;
  }

  already = rh_size;
             
  rh = rfpbuf_at_assert(bgp_ctrl_client->ibuf, 0, sizeof(struct rfp_header));
  length = ntohs(rh->length);
  type = rh->type;
  rfpbuf_prealloc_tailroom(bgp_ctrl_client->ibuf, length - already);  // make sure we have enough space for the body

  if(already < length)
  {
    if(((nbyte = rfpbuf_read_try(bgp_ctrl_client->ibuf, bgp_ctrl_client->sock, length - already)) == 0) ||
       (nbyte == -1))
    {   
//      return ctrl_client_failed(ctrl_client);
    }   

    if(nbyte != (ssize_t) length - already)
    {   
      /* Try again later */
      bgp_ctrl_client_event(BGP_CTRL_CLIENT_READ, bgp_ctrl_client);
      return 0;
    }
  }

  length -= rh_size;

  switch(type)
  {
    case RFPT_HELLO:
      printf("Hello message received\n");

      if(bgp_ctrl_client->state == BGP_CTRL_RECV_HELLO)
        bgp_ctrl_client->state = BGP_CTRL_CONNECTED;

      bgp_ctrl_client_event(BGP_CTRL_CLIENT_CONNECTED, bgp_ctrl_client);
      break;

    case RFPT_FEATURES_REPLY:
      ret = bgp_ctrl_client->features_reply(bgp_ctrl_client, bgp_ctrl_client->ibuf);
      break;

    case RFPT_IPV4_ROUTE_ADD:
      ret = bgp_ctrl_client->routes_reply(bgp_ctrl_client, bgp_ctrl_client->ibuf);
      break;

    case RFPT_FORWARD_BGP:
      printf("Forwarding message received\n");
      break;

  }

  rfpbuf_delete(bgp_ctrl_client->ibuf);
  bgp_ctrl_client->ibuf = NULL;

  bgp_ctrl_client_event(BGP_CTRL_CLIENT_READ, bgp_ctrl_client);
  return 0;
}

static int bgp_ctrl_client_connected(struct thread * t)
{
  int retval;
  struct bgp_ctrl_client * bgp_ctrl_client = THREAD_ARG(t);

  retval = bgp_ctrl_message_send(bgp_ctrl_client, RFPT_FEATURES_REQUEST, RFP10_VERSION);

  retval = bgp_ctrl_message_send(bgp_ctrl_client, RFPT_REDISTRIBUTE_REQUEST, RFP10_VERSION);

  return 0;
}

static void
bgp_ctrl_client_event(enum event event, struct bgp_ctrl_client * bgp_ctrl_client)
{
  switch(event)
  {
    case BGP_CTRL_CLIENT_SCHEDULE:
      bgp_ctrl_client->t_connect = thread_add_event(master, bgp_ctrl_client_connect, bgp_ctrl_client, 0);
      break;

    case BGP_CTRL_CLIENT_CONNECT:
      break;

    case BGP_CTRL_CLIENT_READ:
      bgp_ctrl_client->t_read = thread_add_read(master, bgp_ctrl_client_read, bgp_ctrl_client, bgp_ctrl_client->sock);
      break;

    case BGP_CTRL_CLIENT_CONNECTED:
      bgp_ctrl_client->t_connected = thread_add_event(master, bgp_ctrl_client_connected, bgp_ctrl_client, 0);
      break;
  }
}
