#include "stdio.h"
#include "stdint.h"
#include "stdbool.h"
#include "errno.h"

#include "../lib/rconn.h"
#include "../lib/socket-util.h"
#include "../lib/routeflow-common.h"
#include "../lib/router.h"
#include "../lib/dblist.h"
#include "../lib/rfpbuf.h"
#include "../lib/vconn.h"

#define MAX_ROUTERS 16
#define MAX_LISTENERS 16

struct router_
{
  struct router * router;
};

static void
new_router(struct router_ *rt, struct vconn *vconn, const char *name);

int main(int argc, char *argv[])
{
  int n_listeners, n_routers;
  struct router_ routers[MAX_ROUTERS];
  struct pvconn * listeners[MAX_LISTENERS];
  struct pvconn * pvconn;
  int retval;
  int i;

  n_listeners = 0;
  n_routers = 0;

  time_init();

  retval = pvconn_open("ptcp:6633", &pvconn, DSCP_DEFAULT);
  printf("controller retval: %d\n", retval);
  if(!retval)
  {
    if(n_listeners >= MAX_LISTENERS)
    {
      printf("max switch %d connections\n", n_listeners);
    }
    listeners[n_listeners++] = pvconn;
  }
 
  while(n_routers > 0 || n_listeners > 0)
  {
    printf("controller listeners: %d\n", n_listeners);
    /* Accept connections on listening vconns. */
    for(i = 0; i < n_listeners && n_routers < MAX_ROUTERS; )
    {
      struct vconn * new_vconn;

      retval = pvconn_accept(listeners[i], RFP10_VERSION, &new_vconn);
      printf("accept retval: %d\n", retval);
      if(!retval || retval == EAGAIN)
      {
        if(!retval)
        {
          printf("new router connection\n");
          new_router(&routers[n_routers++], new_vconn, "tcp");
        }
        i++;
      }
      else
      {
        pvconn_close(listeners[i]);
        listeners[i] = listeners[--n_listeners];
      }
    }

    /* Do some switching work. */
    for(i = 0; i < n_routers; i++)
    {
      struct router_ * this = &routers[i];
      router_run(this->router);
      if(router_is_alive(this->router))
      {
        i++;
      }
      else
      {
        router_destroy(this->router);
        routers[i] = routers[--n_routers];
      }
    }

    /* Wait for something to happen */
    if(n_routers < MAX_ROUTERS)
    {
      for(i = 0; i < n_listeners; i++)
      {
        pvconn_wait(listeners[i]);
      }
    }
    for(i = 0; i < n_routers; i++)
    {
      struct router_ * rt = &routers[i];
      router_wait(rt->router);
    }

    poll_block();
  }
 
  return 0;
}

static void
new_router(struct router_ *rt, struct vconn *vconn, const char *name)
{
    struct rconn * rconn;
  
    rconn = rconn_create();
    rconn_connect_unreliably(rconn, vconn, NULL);
    rt->router = router_create(rconn);
}
