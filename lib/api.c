#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <netinet/in.h>

#include "util.h"
#include "dblist.h"
#include "prefix.h"
#include "netlink.h"
#include "routeflow-common.h"
#include "api.h"

int rib_add_ipv4 (struct route_ipv4 * route, void * data);
#ifdef HAVE_IPV6
int rib_add_ipv6 (struct route_ipv6 * route, void * data);
#endif /* HAVE_IPV6 */

void api_init()
{
  kernel_init();
}

int routes_list(struct list * ipv4_rib_routes, struct list * ipv6_rib_routes, int (*rib_add_ipv4)(struct route_ipv4 *, void *), int (*rib_add_ipv6)(struct route_ipv6 *, void *))
{
  // Set up callbacks
  struct netlink_routing_table_info info;
  memset(&info, 0, sizeof(info));
  info.rib_add_ipv4_route = rib_add_ipv4;
  info.ipv4_rib = ipv4_rib_routes;
  #ifdef HAVE_IPV6
  info.rib_add_ipv6_route = rib_add_ipv6;
  info.ipv6_rib = ipv6_rib_routes;
  #endif /* HAVE_IPV6 */

  netlink_route_read(&info);

  return 0;
}

int addrs_list(struct list * ipv4_addr, struct list * ipv6_addr, int (*add_ipv4_addr)(int index, void * address, u_char prefixlen, struct list * list),
                                                                 int (*add_ipv6_addr)(int index, void * address, u_char prefixlen, struct list * list))
{
  struct netlink_addrs_info info;
  memset(&info, 0, sizeof(info));
  info.ipv4_addrs = ipv4_addr;
  info.add_ipv4_addr = add_ipv4_addr;
  #ifdef HAVE_IPV6
  info.ipv6_addrs = ipv6_addr;
  info.add_ipv6_addr = add_ipv6_addr;
  #endif 
 
  netlink_addr_read(&info);

  return 0;
}

int interface_list(struct list * list, int (*add_port)(int index, unsigned int flags, unsigned int mtu, char * name, struct list * list))
{
  // set up callbacks
  struct netlink_port_info info;
  memset(&info, 0, sizeof(info));
  info.add_port = add_port;
  info.all_ports = list;
  netlink_link_read(&info);

  return 0;
}
