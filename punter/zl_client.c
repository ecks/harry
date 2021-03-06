#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>

#include "routeflow-common.h"
#include "util.h"
#include "dblist.h"
#include "rfpbuf.h"
#include "thread.h"
#include "punter_ctrl.h"
#include "zl_client_network.h"
#include "zl_client.h"

enum event {ZL_CLIENT_SCHEDULE, ZL_CLIENT_READ};

static void zl_client_event(enum event, struct zl_client *);
static int zl_client_failed(struct zl_client *);

extern struct thread_master * master;

struct zl_client * zl_client_new()
{
  return calloc(1, sizeof(struct zl_client));
}

void zl_client_init(struct zl_client * zl_client, struct punter_ctrl * punter_ctrl)
{
  zl_client->sockfd = -1;
  zl_client->punter_ctrl = punter_ctrl;

  zl_client->ibuf = NULL;
  zl_client_event(ZL_CLIENT_SCHEDULE, zl_client);
}

static int zl_client_start(struct zl_client * zl_client)
{
  int retval;

  if(zl_client->sockfd >= 0)
    return 0;

  if(zl_client->t_connect)
    return 0;

  if((zl_client->sockfd = zl_client_sock_init(ZL_SOCKET_PATH)) < 0)
  {
    printf("zl_client connection fail\n");
    return -1;
  }

  zl_client_event(ZL_CLIENT_READ, zl_client);

  return 0;
}

static int zl_client_connect(struct thread * t)
{
  struct zl_client * zl_client = NULL;

  zl_client = THREAD_ARG(t);
  zl_client->t_connect = NULL;

  return zl_client_start(zl_client);
}

static int zl_client_read(struct thread * t)
{
  struct zl_client * zl_client = THREAD_ARG(t);
  struct rfp_header * rh;
  size_t rh_size = sizeof(struct rfp_header);
  struct ospf6_header * oh;
  uint16_t oh_msg_length;
  uint16_t length;
  uint32_t xid;
  uint8_t type;
  size_t already = 0;
  ssize_t nbyte;

  if(zl_client->ibuf == NULL)
  {
    zl_client->ibuf = rfpbuf_new(rh_size);
  }
  else
  {
    // this should never happen
    assert(1 == 0);
//    rfpbuf_put_uninit(zl_client->ibuf, rh_size);
  }

  if(((nbyte = zl_client_network_recv(zl_client->ibuf, zl_client->sockfd, rh_size - already)) == 0) ||
      (nbyte == -1))
  { 
    return zl_client_failed(zl_client);
  }
  if(nbyte != rh_size - already)
  {
    /* Try again later */
    zl_client_event(ZL_CLIENT_READ, zl_client);
    return 0;
  }

  already = rh_size;

  rh = rfpbuf_at_assert(zl_client->ibuf, 0, sizeof(struct rfp_header));
  length = ntohs(rh->length);
  xid = ntohl(rh->xid);
  type = rh->type;
  rfpbuf_prealloc_tailroom(zl_client->ibuf, length - already);

  if(already < length)
  {
    if(((nbyte = zl_client_network_recv(zl_client->ibuf, zl_client->sockfd, length - already)) == 0) ||
               (nbyte == -1))
    {   
      zl_client_failed(zl_client); 
      return -1;
    }   

    if(nbyte != (ssize_t) length - already)
    {   
      /* Try again later */
      zl_client_event(ZL_CLIENT_READ, zl_client);
      return 0;
    }   
  }

  length -= rh_size;

  switch(type)
  {
    case RFPT_FORWARD_OSPF6:
      printf("Received OSPF6 handling traffic with length: %d and xid: %d\n", length, xid);
      rh = rfpbuf_at_assert(zl_client->ibuf, 0, sizeof(struct rfp_header));

      oh = (struct ospf6_header *)((void *)rh + sizeof(struct rfp_header));
      oh_msg_length = ntohs(oh->length);

      switch(oh->type)
      {
        case OSPF6_MESSAGE_TYPE_HELLO:
          printf("About to send HELLO\n");
          break; 

        case OSPF6_MESSAGE_TYPE_DBDESC:
          printf("About to send DBDESC\n");
          break;

        case OSPF6_MESSAGE_TYPE_LSREQ:
          printf("About to send LSREQ\n");
          break;

        case OSPF6_MESSAGE_TYPE_LSUPDATE:
          printf("About to send LSUPDATE\n");
          break;

        case OSPF6_MESSAGE_TYPE_LSACK:
          printf("About to send LSACK\n");
          break;

        default:
          printf("About to send msg with unknown type\n");
          break;

      }

      if(zl_client->obuf == NULL)
      {
        zl_client->obuf = rfpbuf_new(oh_msg_length);
      }
      else
      {
        // should never happen
        assert(1 == 0);
      }

      rfpbuf_put(zl_client->obuf, (void *)oh, oh_msg_length);  
      punter_zl_to_ext_forward_msg(type);

      // clean up after finished sending message
      rfpbuf_delete(zl_client->obuf);
      zl_client->obuf = NULL;
      break;

    default:
      return -1;
  }

  rfpbuf_delete(zl_client->ibuf);
  zl_client->ibuf = NULL;

  zl_client_event(ZL_CLIENT_READ, zl_client);
  return 0;
}

static void zl_client_stop(struct zl_client * zl_client)
{
  /* Stop the threads */
  THREAD_OFF(zl_client->t_connect);
  THREAD_OFF(zl_client->t_read);

  rfpbuf_delete(zl_client->ibuf);
  zl_client->ibuf = NULL;

  if(zl_client->sockfd >= 0)
  {
    close(zl_client->sockfd);
    zl_client->sockfd = -1;
  }
}
  

static int zl_client_failed(struct zl_client * zl_client)
{
  zl_client->fail++;
  zl_client_stop(zl_client);
  zl_client_event(ZL_CLIENT_SCHEDULE, zl_client);
  return -1;
}

static void zl_client_event(enum event event, struct zl_client * zl_client)
{
  switch(event)
  {
    case ZL_CLIENT_SCHEDULE:
      zl_client->t_connect = thread_add_event(master, zl_client_connect, zl_client, 0);
      break;

   case ZL_CLIENT_READ:
     zl_client->t_read = thread_add_read(master, zl_client_read, zl_client, zl_client->sockfd);
     break;

  default:
    break;
  }
}
