#include "stdio.h"
#include "stdlib.h"
#include "stdint.h"
#include "string.h"
#include <stdbool.h>
#include "netdb.h"
#include "netinet/in.h"
#include "sys/socket.h"

#include "dblist.h"
#include "prefix.h"
#include "rfpbuf.h"
#include "rfp-msgs.h"
#include "routeflow-common.h"
#include "thread.h"
#include "ctrl_client.h"

// TODO: need to make global
#define CTRL_SISIS_PORT 6634

enum ctrl_client_state
{
  CTRL_CONNECTING,        /* not connected */
  CTRL_SEND_HELLO,        /* waiting to send HELLO */
  CTRL_RECV_HELLO,        /* waiting to receive HELLO */
  CTRL_CONNECTED          /* connectionn established */
};

enum event {CTRL_CLIENT_SCHEDULE, CTRL_CLIENT_CONNECT, CTRL_CLIENT_READ, CTRL_CLIENT_CONNECTED};

/* Prototype for event manager */
static void ctrl_client_event(enum event, struct ctrl_client * ctrl_client);

extern struct thread_master * master;

struct ctrl_client * ctrl_client_new()
{
  struct ctrl_client * ctrl_client = calloc(1, sizeof(*ctrl_client));
  return ctrl_client;
}

void ctrl_client_init(struct ctrl_client * ctrl_client, struct in6_addr * ctrl_addr)
{
  ctrl_client->sock = -1;

  ctrl_client->ctrl_addr = ctrl_addr;

  ctrl_client->state = CTRL_CONNECTING;

  ctrl_client_event(CTRL_CLIENT_SCHEDULE, ctrl_client);
}

void ctrl_client_stop(struct ctrl_client * ctrl_client)
{
  /* Stop threads */
  THREAD_OFF(ctrl_client->t_read);
  THREAD_OFF(ctrl_client->t_write);
  THREAD_OFF(ctrl_client->t_connect);

  rfpbuf_delete(ctrl_client->ibuf);
  ctrl_client->ibuf = NULL;

  rfpbuf_delete(ctrl_client->obuf);
  ctrl_client->ibuf = NULL;

  if(ctrl_client->sock >= 0)
  {
    close(ctrl_client->sock);
    ctrl_client->sock = -1;
  }

  ctrl_client->fail = 0;
}

int ctrl_client_socket(struct in6_addr * ctrl_addr)
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
  sprintf(port_str, "%u", CTRL_SISIS_PORT);
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

  printf("ctrl_client TCP socket: creating socket %d\n", sock);

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

static int
ctrl_client_failed(struct ctrl_client * ctrl_client)
{
  ctrl_client->fail++;
  ctrl_client_stop(ctrl_client);
  ctrl_client_event(CTRL_CLIENT_CONNECT, ctrl_client);
  return -1;
}

int ctrl_send_message(struct ctrl_client * ctrl_client)
{
  if(ctrl_client->sock < 0)
    return -1;
  switch(rfpbuf_write(ctrl_client->obuf, ctrl_client->sock))
  {
    case RFPBUF_ERROR:
      break;

    case RFPBUF_EMPTY:
      break;
  }

  rfpbuf_delete(ctrl_client->obuf);
  ctrl_client->obuf = NULL;

  return 0;
}

/* Send simple message 
   where only header is 
   necessary to be filled */
static int ctrl_message_send(struct ctrl_client * ctrl_client, uint8_t type, uint8_t version)
{
  ctrl_client->obuf = routeflow_alloc(type, version, sizeof(struct rfp_header));

  return ctrl_send_message(ctrl_client);  
}

/* connection to controller daemon */
int ctrl_client_start(struct ctrl_client * ctrl_client)
{
  int retval;

  if(ctrl_client->sock >= 0)
    return 0;

  if(ctrl_client->t_connect)
    return 0;

  /* Make socket */
  ctrl_client->sock = ctrl_client_socket (ctrl_client->ctrl_addr);
  if (ctrl_client->sock < 0)
  {
    printf("ctrl_client connection fail\n");
    ctrl_client->fail++;
    ctrl_client_event (CTRL_CLIENT_CONNECT, ctrl_client);
    return -1;
  }

  if(set_nonblocking(ctrl_client->sock) < 0)
  {
    printf("set_nonblocking failed\n");
  }

  /* Clear fail count */
  ctrl_client->fail = 0;

  ctrl_client_event(CTRL_CLIENT_READ, ctrl_client);

  retval = ctrl_message_send(ctrl_client, RFPT_HELLO, RFP10_VERSION);
  if(!retval)
    ctrl_client->state = CTRL_RECV_HELLO;
  
  return 0; 
}

/* wrapper function for threads */
static int ctrl_client_connect(struct thread * t)
{
  struct ctrl_client * ctrl_client;

  ctrl_client = THREAD_ARG(t);
  ctrl_client->t_connect = NULL;

  return ctrl_client_start(ctrl_client);
}

static int ctrl_client_read(struct thread * t)
{
  int ret;
  size_t already = 0;
  struct rfp_header * rh;
  uint16_t length;
  uint8_t type;
  size_t rh_size = sizeof(struct rfp_header);
  struct ctrl_client * ctrl_client = THREAD_ARG(t);
  ctrl_client->t_read = NULL;

  if(ctrl_client->ibuf == NULL)
  {
    ctrl_client->ibuf = rfpbuf_new(rh_size);
  }
  else
  {
    rfpbuf_put_uninit(ctrl_client->ibuf, rh_size);
  }

  ssize_t nbyte;
  if(((nbyte = rfpbuf_read_try(ctrl_client->ibuf, ctrl_client->sock, sizeof(struct rfp_header)-already)) == 0) ||
     (nbyte == -1))
  {
    return ctrl_client_failed(ctrl_client);
  }
  if(nbyte != (ssize_t)(sizeof(struct rfp_header)-already))
  {
    /* Try again later */
    ctrl_client_event(CTRL_CLIENT_READ, ctrl_client);
    return 0;
  }

  already = rh_size;
 
  rh = rfpbuf_at_assert(ctrl_client->ibuf, 0, sizeof(struct rfp_header));
  length = ntohs(rh->length);
  type = rh->type;
  rfpbuf_prealloc_tailroom(ctrl_client->ibuf, length - already);  // make sure we have enough space for the body

  if(already < length)
  {
    if(((nbyte = rfpbuf_read_try(ctrl_client->ibuf, ctrl_client->sock, length - already)) == 0) ||
       (nbyte == -1))
    {
      return ctrl_client_failed(ctrl_client);
    }

    if(nbyte != (ssize_t) length - already)
    {
      /* Try again later */
      ctrl_client_event(CTRL_CLIENT_READ, ctrl_client);
      return 0;
    }
  }

  length -= rh_size;

  switch(type)
  {
    case RFPT_HELLO:
      printf("Hello message received\n");

      if(ctrl_client->state == CTRL_RECV_HELLO)
        ctrl_client->state == CTRL_CONNECTED;
  
      ctrl_client_event(CTRL_CLIENT_CONNECTED, ctrl_client);
      break;
    
    case RFPT_FEATURES_REPLY:
      ret = ctrl_client->features_reply(ctrl_client, ctrl_client->ibuf);
      break;

    case RFPT_GET_CONFIG_REPLY:
      break;

    case RFPT_IPV4_ROUTE_ADD:
      ret = ctrl_client->routes_reply(ctrl_client, ctrl_client->ibuf);
      break;

    case RFPT_FORWARD_OSPF6:
      printf("Forwarding message received\n");
      break;
  }

  rfpbuf_delete(ctrl_client->ibuf);
  ctrl_client->ibuf = NULL;

  ctrl_client_event(CTRL_CLIENT_READ, ctrl_client);
  return 0;
}

static int ctrl_client_connected(struct thread * t)
{
  int retval;
  struct ctrl_client * ctrl_client = THREAD_ARG(t);

  retval = ctrl_message_send(ctrl_client, RFPT_FEATURES_REQUEST, RFP10_VERSION);
  
  retval = ctrl_message_send(ctrl_client, RFPT_REDISTRIBUTE_REQUEST, RFP10_VERSION);

  return 0;
}

static void 
ctrl_client_event(enum event event, struct ctrl_client * ctrl_client)
{
  switch(event)
  {
    case CTRL_CLIENT_SCHEDULE:
      ctrl_client->t_connect = thread_add_event(master, ctrl_client_connect, ctrl_client, 0);
      break;
    case CTRL_CLIENT_CONNECT:
      // TODO: implement connecting again
      break;
    case CTRL_CLIENT_READ:
      ctrl_client->t_read = thread_add_read(master, ctrl_client_read, ctrl_client, ctrl_client->sock);
      break;
    case CTRL_CLIENT_CONNECTED:
      ctrl_client->t_connected = thread_add_event(master, ctrl_client_connected, ctrl_client, 0);
      break;
  }
}
