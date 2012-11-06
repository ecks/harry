#include "stdio.h"
#include "stdint.h"
#include "stdbool.h"
#include "stdlib.h"
#include "errno.h"

#include "rconn.h"
#include "socket-util.h"
#include "routeflow-common.h"
#include "router.h"
#include "sib_router.h"
#include "dblist.h"
#include "rfpbuf.h"
#include "vconn.h"
#include "sisis.h"
#include "sisis_process_types.h"

#define MAX_ROUTERS 16
#define MAX_LISTENERS 16
#define MAX_SIBLINGS 3

struct sib_router_
{
  struct sib_router * sib_router;
};

struct router_
{
  struct router * router;
};

static void
new_router(struct router ** rt, struct vconn *vconn, const char *name);

static void
new_sib_router(struct sib_router ** sr, struct vconn *vconn, const char *name, struct router ** routers, int n_routers);

int main(int argc, char *argv[])
{
  int n_nb_listeners, n_sib_listeners, n_routers, n_siblings;
  struct router * routers[MAX_ROUTERS];
  struct sib_router * siblings[MAX_SIBLINGS];
  struct pvconn * nb_listeners[MAX_LISTENERS];
  struct pvconn * sib_listeners[MAX_LISTENERS];
  struct pvconn * nb_pvconn;   // north-bound interface connection
  struct pvconn * sib_pvconn;  // sibling channel interface
  int retval;
  int i;

  n_nb_listeners = 0;
  n_sib_listeners = 0;
  n_routers = 0;
  n_siblings = 0;

  // TODO: set up signal handlers
//  signal(SIGABRT, );
//  signal(SIGTERM, );
//  signal(SIGINT, );

  time_init();
  rib_init();

  // initialize internal socket to sisis
  int sisis_fd;
  uint64_t host_num = 1;

  if((sisis_fd = sisis_init(host_num, SISIS_PTYPE_CTRL)) < 0)
  {
    printf("sisis_init error\n");
    exit(1);
  }
  
  retval = pvconn_open("ptcp:6633", &nb_pvconn, DSCP_DEFAULT);
  if(!retval)
  {
    if(n_nb_listeners >= MAX_LISTENERS)
    {
      printf("max switch %d connections\n", n_nb_listeners);
    }
    nb_listeners[n_nb_listeners++] = nb_pvconn;
  }

  retval = pvconn_open("ptcp6:6634", &sib_pvconn, DSCP_DEFAULT);
  if(!retval)
  {
    if(n_sib_listeners >= MAX_LISTENERS)
    {
      printf("max switch %d connections\n", n_sib_listeners);
    }
    sib_listeners[n_sib_listeners++] = sib_pvconn;
  }

  while(n_siblings > 0 || n_nb_listeners > 0)
  {
    /* Accept connections on listening sibling interfaces */
    for(i = 0; i < n_sib_listeners && n_siblings < MAX_SIBLINGS; )
    {
      struct vconn * new_sib_vconn;

      retval = pvconn_accept(sib_listeners[i], RFP10_VERSION, &new_sib_vconn);
      printf("accept retval: %d\n", retval);
      if(!retval || retval == EAGAIN)
      {
        if(!retval)
        {
          printf("new sibling connection\n");
          // TODO: expect only one router connection for now
          if(n_routers == 1)
          {
            new_sib_router(&siblings[n_siblings++], new_sib_vconn, "tcp", routers, n_routers);
          }
          else
          {
            new_sib_router(&siblings[n_siblings++], new_sib_vconn, "tcp", NULL, n_routers);
          }
        }
        i++;
      }
      else
      {
        pvconn_close(sib_listeners[i]);
        sib_listeners[i] = sib_listeners[--n_sib_listeners];
      }
    }

    /* Accept connections on listening north-bound interfaces. */
    for(i = 0; i < n_nb_listeners && n_routers < MAX_ROUTERS; )
    {
      struct vconn * new_nb_vconn;

      retval = pvconn_accept(nb_listeners[i], RFP10_VERSION, &new_nb_vconn);
      printf("accept retval: %d\n", retval);
      if(!retval || retval == EAGAIN)
      {
        if(!retval)
        {
          printf("new router connection\n");
          new_router(&routers[n_routers++], new_nb_vconn, "tcp");
        }
        i++;
      }
      else
      {
        pvconn_close(nb_listeners[i]);
        nb_listeners[i] = nb_listeners[--n_nb_listeners];
      }
    }

    /* Do some switching work. */
    for(i = 0; i < n_siblings; )
    {
      struct sib_router * sib_router = siblings[i];
      sib_router_run(sib_router);
      if(sib_router_is_alive(sib_router))
      {
        i++;
      }
      else
      {
        sib_router_destroy(sib_router);
        siblings[i] = siblings[--n_siblings];
      }
    }

    /* Do some switching work. */
    for(i = 0; i < n_routers; )
    {
      struct router * router = routers[i];
      router_run(router);
      if(router_is_alive(router))
      {
        i++;
      }
      else
      {
        router_destroy(router);
        routers[i] = routers[--n_routers];
      }
    }

    /* Wait for something to happen */
    if(n_siblings < MAX_SIBLINGS)
    {
      for(i = 0; i < n_sib_listeners; i++)
      {
        pvconn_wait(sib_listeners[i]);
      }
    }

    if(n_routers < MAX_ROUTERS)
    {
      for(i = 0; i < n_nb_listeners; i++)
      {
        pvconn_wait(nb_listeners[i]);
      }
    }

    for(i = 0; i < n_siblings; i++)
    {
      struct sib_router * sib_router = siblings[i];
      sib_router_wait(sib_router);
    }

    for(i = 0; i < n_routers; i++)
    {
      struct router * router = routers[i];
      router_wait(router);
    }

    poll_block();
  }
 
  return 0;
}

static void
new_router(struct router ** rt, struct vconn *vconn, const char *name)
{
    struct rconn * rconn;
  
    rconn = rconn_create();
    rconn_connect_unreliably(rconn, vconn, NULL);
    *rt = router_create(rconn);
}

static void
new_sib_router(struct sib_router ** sr, struct vconn *vconn, const char *name, struct router ** rt, int n_routers)
{
    struct rconn * rconn;
  
    rconn = rconn_create();
    rconn_connect_unreliably(rconn, vconn, NULL);

    // expect to have only one interface for now
    if(n_routers == 1)
    {
      *sr = sib_router_create(rconn, rt, n_routers);
    }
    else
    {
      *sr = sib_router_create(rconn, NULL, n_routers);
    }
}
