#include "config.h"

#include "assert.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "stdbool.h"
#include "stddef.h"
#include "errno.h"
#include "sys/socket.h"
#include "netinet/in.h"
#include "sys/types.h"

#include "routeflow-common.h"
#include "util.h"
#include "dblist.h"
#include "prefix.h"
#include "punter_ctrl.h"
#include "ext_client_ospf6.h"
#include "ext_client_network.h"

unsigned int ext_client_ospf6_mtu(struct list * ports, unsigned int ifindex)
{
  unsigned int mtu;

  struct sw_port * port;
  LIST_FOR_EACH(port, struct sw_port, node, ports)
  {
    if(port->port_no == ifindex)
    {
      mtu = port->mtu;
    }
  }
 
  return mtu;
}
