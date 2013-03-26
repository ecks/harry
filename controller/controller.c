#include "stdio.h"
#include "stdint.h"
#include "stdbool.h"
#include "stdlib.h"
#include "errno.h"

#include "socket-util.h"
#include "routeflow-common.h"
#include "dblist.h"
#include "thread.h"
#include "rconn.h"
#include "router.h"
#include "sib_router.h"
#include "rfpbuf.h"
#include "vconn.h"
#include "sisis.h"
#include "sisis_process_types.h"

#define MAX_ROUTERS 16
#define MAX_LISTENERS 16
#define MAX_SIBLINGS 3

struct thread_master * master;

struct sib_router_
{
  struct sib_router * sib_router;
};

struct router_
{
  struct router * router;
};

static struct router * routers[MAX_ROUTERS];
int n_routers;

static struct sib_router * siblings[MAX_SIBLINGS];
int n_siblings;

static int nb_listener_accept(struct thread * t);

static void
new_router(struct router **, struct vconn *, const char *, struct sib_router **, int *);

static int sib_listener_accept(struct thread * t);

static void
new_sib_router(struct sib_router **, struct vconn *, const char *, struct router **, int *);

int main(int argc, char *argv[])
{
  struct thread thread;
  int n_nb_listeners, n_sib_listeners;
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

  /* thread master */
  master = thread_master_create();

  time_init();
  rib_init();
  if_init();

  // initialize internal socket to sisis
  int sisis_fd;
  uint64_t host_num = 1;

  if((sisis_fd = sisis_init(host_num, SISIS_PTYPE_CTRL)) < 0)
  {
    printf("sisis_init error\n");
    exit(1);
  }
  
  retval = pvconn_open("ptcp6:6633", &nb_pvconn, DSCP_DEFAULT);
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

//  while(n_siblings > 0 || n_nb_listeners > 0)
//  {
    /* Accept connections on listening sibling interfaces */
//    for(i = 0; i < n_sib_listeners && n_siblings < MAX_SIBLINGS; )
//    {
//      struct vconn * new_sib_vconn;

//      retval = pvconn_accept(sib_listeners[i], RFP10_VERSION, &new_sib_vconn);
//      printf("accept retval: %d\n", retval);
//      if(!retval || retval == EAGAIN)
//      {
//        if(!retval)
//        {
//          printf("new sibling connection\n");
          // TODO: expect only one router connection for now
//          if(n_routers == 1)
//          {
//            new_sib_router(&siblings[n_siblings++], new_sib_vconn, "tcp", routers, &n_routers);
//          }
//          else
//          {
//            new_sib_router(&siblings[n_siblings++], new_sib_vconn, "tcp", NULL, &n_routers);
//          }
//        }
//        i++;
//      }
//      else
//      {
//        pvconn_close(sib_listeners[i]);
//        sib_listeners[i] = sib_listeners[--n_sib_listeners];
//      }
//    }

    /* Accept connections on listening north-bound interfaces. */
//    for(i = 0; i < n_nb_listeners && n_routers < MAX_ROUTERS; )
//    {
//      struct vconn * new_nb_vconn;

//      retval = pvconn_accept(nb_listeners[i], RFP10_VERSION, &new_nb_vconn);
//      printf("accept retval: %d\n", retval);
//      if(!retval || retval == EAGAIN)
//      {
//        if(!retval)
//        {
//          printf("new router connection\n");
//          new_router(&routers[n_routers++], new_nb_vconn, "tcp", siblings, &n_siblings);
//        }
//        i++;
//      }
//      else
//      {
//        pvconn_close(nb_listeners[i]);
//        nb_listeners[i] = nb_listeners[--n_nb_listeners];
//      }
//    }

    /* Wait for something to happen */
    if(n_siblings < MAX_SIBLINGS)
    {
      for(i = 0; i < n_sib_listeners; i++)
      {
        pvconn_wait(sib_listeners[i], sib_listener_accept, sib_listeners[i]);
      }
    }

    if(n_routers < MAX_ROUTERS)
    {
      for(i = 0; i < n_nb_listeners; i++)
      {
        pvconn_wait(nb_listeners[i], nb_listener_accept, nb_listeners[i]);
      }
    }

    // this is ineffective since there are no siblings yet
    for(i = 0; i < n_siblings; i++)
    {
      struct sib_router * sib_router = siblings[i];
      sib_router_wait(sib_router);
    }

    // this is ineffective since there are no routers yet
    for(i = 0; i < n_routers; i++)
    {
      struct router * router = routers[i];
      router_wait(router);
    }

//    poll_block();
  
  // spin forever
  while(thread_fetch(master, &thread))
  {
    thread_call(&thread);
  }
//  }
 
  return 0;
}

static int nb_listener_accept(struct thread * t)
{
  struct pvconn * nb_listener = THREAD_ARG(t);
  struct vconn * new_nb_vconn;
  int retval;

  retval = pvconn_accept(nb_listener, RFP10_VERSION, &new_nb_vconn);
  if(!retval)
  {
    printf("new router connection\n");
    new_router(&routers[n_routers++], new_nb_vconn, "tcp6", siblings, &n_siblings);
  }
  else if(retval != EAGAIN)
  {
    pvconn_close(nb_listener);
  }

  // reregister ourselves
  pvconn_wait(nb_listener, nb_listener_accept, nb_listener);

  return 0;
}


static void
new_router(struct router ** rt, struct vconn *vconn, const char *name, struct sib_router ** srt, int * n_siblings_p)
{
    struct rconn * rconn;
  
    rconn = rconn_create();
    rconn_connect_unreliably(rconn, vconn, NULL);
    *rt = router_create(rconn, srt, n_siblings_p);

    // schedule an event to call router_run
    thread_add_event(master, router_run, *rt, 0);
}

static int sib_listener_accept(struct thread * t)
{
  struct pvconn * sib_listener = THREAD_ARG(t);
  struct vconn * new_sib_vconn;
  int retval;

  retval = pvconn_accept(sib_listener, RFP10_VERSION, &new_sib_vconn);
  if(!retval)
  {
    printf("new sibling connection\n");
    // TODO: expect only one router connection for now
    if(n_routers == 1)
    {
      new_sib_router(&siblings[n_siblings++], new_sib_vconn, "tcp6", routers, &n_routers);
    }
    else
    {
      new_sib_router(&siblings[n_siblings++], new_sib_vconn, "tcp6", NULL, &n_routers);
    }
  }
  else if(retval != EAGAIN)
  {
    pvconn_close(sib_listener);
  }

  // reregister ourselves
  pvconn_wait(sib_listener, sib_listener_accept, sib_listener);

  return 0;
}

static void
new_sib_router(struct sib_router ** sr, struct vconn *vconn, const char *name, struct router ** rt, int * n_routers_p)
{
    struct rconn * rconn;
  
    rconn = rconn_create();
    rconn_connect_unreliably(rconn, vconn, NULL);

    // expect to have only one interface for now
    if(*n_routers_p == 1)  // dereference the pointer
    {
      *sr = sib_router_create(rconn, rt, n_routers_p);

      // schedule an event to call sib_router_run
      thread_add_event(master, sib_router_run, *sr, 0);
    }
    else
    {
      *sr = sib_router_create(rconn, NULL, n_routers_p);
    }
}
