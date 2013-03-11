#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "routeflow-common.h"
#include "util.h"
#include "dblist.h"
#include "thread.h"
#include "if.h"
#include "ospf6_interface.h"
#include "ospf6_top.h"

extern struct thread_master * master;

void ospf6_interface_area(char * interface)
{
  struct interface * ifp;
  struct ospf6_interface * oi;

  /* find/create ospf6 interface */
  ifp = if_get_by_name(interface);
  oi = (struct ospf6_interface *) ifp->info;
  if(oi == NULL)
  {
    oi = ospf6_interface_create(ifp);
  }

  /* start up */
  thread_add_event(master, interface_up, oi, 0);
}

void ospf6_top_init(char * interface)
{
  ospf6_interface_area(interface);
}
