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

int rib_add_ipv4 (struct route_ipv4 * route, struct list * list);
#ifdef HAVE_IPV6
int rib_add_ipv6 (struct route_ipv6 * route, struct list * list);
#endif /* HAVE_IPV6 */

void api_init()
{
  kernel_init();
}

int routes_list(struct list * ipv4_rib_routes, struct list * ipv6_rib_routes)
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

  struct route_ipv4 * route;
  LIST_FOR_EACH(route, struct route_ipv4, node, ipv4_rib_routes)
  {
    // print route
    char prefix_str[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &(route->p->prefix.s_addr), prefix_str, INET_ADDRSTRLEN) != 1)
  	printf("%s/%d [%u/%u]\n", prefix_str, route->p->prefixlen, route->distance, route->metric);
  }

  return 0;
}

int addrs_list(struct list * ipv4_addrs, struct list * ipv6_addrs, int (*add_ipv4_addr)(int index, void * address, u_char prefixlen, struct list * list), int (*remove_ipv4_addr)(int index, struct list * list),
                                                                   int (*add_ipv6_addr)(int index, void * address, u_char prefixlen, struct list * list), int (*remove_ipv6_addr)(int index, struct list * list))
{
  struct netlink_addrs_info info;
  memset(&info, 0, sizeof(info));
  info.ipv4_addrs = ipv4_addrs;
  info.add_ipv4_addr = add_ipv4_addr;
  info.remove_ipv4_addr = remove_ipv4_addr;
  #ifdef HAVE_IPV6
  info.ipv6_addrs = ipv6_addrs;
  info.add_ipv6_addr = add_ipv6_addr;
  info.remove_ipv6_addr = remove_ipv6_addr;
  #endif 
 
  netlink_addr_read(&info);
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

/* Add an IPv4 Address to RIB. */
int rib_add_ipv4 (struct route_ipv4 * route, struct list * list)
{
  list_push_back(list, &route->node);

  return 0;
}

#ifdef HAVE_IPV6
int rib_add_ipv6 (struct route_ipv6 * route, struct list * list)
{
  list_push_back(list, &route->node);
  return 0;
}
#endif /* HAVE_IPV6 */
