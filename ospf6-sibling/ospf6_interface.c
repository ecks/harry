#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>

#include "routeflow-common.h"
#include "util.h"
#include "dblist.h"
#include "thread.h"
#include "if.h"
#include "ospf6_message.h"
#include "ospf6_interface.h"

extern struct thread_master * master;

/* Create new ospf6 interface structure */
struct ospf6_interface * ospf6_interface_create (struct interface *ifp)
{
    struct ospf6_interface *oi;

    oi = calloc(1, sizeof(struct ospf6_interface));

    oi->area = NULL;
    oi->priority = 1;

    oi->hello_interval = 10;
    oi->dead_interval = 40;
    oi->rxmt_interval = 5;
    
    oi->state = OSPF6_INTERFACE_DOWN;
    
    oi->interface = ifp;
    ifp->info = oi;

    return oi;
}

void ospf6_interface_if_add(struct interface * ifp, struct ctrl_client * ctrl_client)
{
 struct ospf6_interface * oi = (struct ospf6_interface *)ifp->info;
  if(oi == NULL)
    return;

  oi->ctrl_client = ctrl_client;
  /* Interface start */
  thread_add_event(master, interface_up, oi, 0);
}

int interface_up(struct thread * thread)
{
  struct ospf6_interface * oi;

  oi = THREAD_ARG(thread);
  assert(oi && oi->interface);
  
  /* check that physical interface is up */
  if( !if_is_up(oi->interface))
  {
    return 0;
  }

  /* if already enabled, do nothing */
  if(oi->state > OSPF6_INTERFACE_DOWN)
  {
    return 0;
  }

  /* Schedule Hello */
  thread_add_event(master, ospf6_hello_send, oi, 0);

  return 0;
}
