#include "config.h"

#include <stdlib.h>
#include <stdio.h>
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

static void zl_client_event(enum event event, struct zl_client * zl_client);

extern struct thread_master * master;

struct zl_client * zl_client_new()
{
  return calloc(1, sizeof(struct zl_client));
}

void zl_client_init(struct zl_client * zl_client, struct punter_ctrl * punter_ctrl)
{
  zl_client->sockfd = -1;
  zl_client->punter_ctrl = punter_ctrl;

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
  int nbytes;

  zl_client_event(ZL_CLIENT_READ, zl_client);

//  nbytes = zl_client_read(zl_client);

  return 0;
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
