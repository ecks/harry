#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <netinet/in.h>

#include "lib/Class.h"
#include "lib/RouteV4.h"
#include "lib/util.h"
#include "lib/dblist.h"
#include "lib/ip.h"
#include "netlink.h"
#include "lib/routeflow-common.h"
#include "datapath.h"
#include "api.h"

int rib_add_ipv4 (struct route_ipv4 * route, struct list * list);
#ifdef HAVE_IPV6
int rib_add_ipv6 (struct route_ipv6 * route, struct list * list);
#endif /* HAVE_IPV6 */

int iface_add_port (struct sw_port * port, struct list * list);

void api_init()
{
  kernel_init();
}

int api_read_kernel_routes()
{

  // IPv4/IPv6 Ribs
  struct list ipv4_rib_routes;

#ifdef HAVE_IPV6
  struct list ipv6_rib_routes;
#endif /* HAVE_IPV6 */


  // Set up list of IPv4 rib entries
//  if (ipv4_rib_routes)
//    FREE_LINKED_LIST(ipv4_rib_routes);

  list_init(&ipv4_rib_routes);

#ifdef HAVE_IPV6
  // Set up list of IPv6 rib entries
//  if (ipv6_rib_routes)
//    FREE_LINKED_LIST(ipv6_rib_routes);

  list_init(&ipv6_rib_routes);
#endif /* HAVE_IPV6 */

  // Set up callbacks
  struct netlink_routing_table_info info;
  memset(&info, 0, sizeof(info));
  info.rib_add_ipv4_route = rib_add_ipv4;
  info.ipv4_rib = &ipv4_rib_routes;
  #ifdef HAVE_IPV6
  info.rib_add_ipv6_route = rib_add_ipv6;
  info.ipv6_rib = &ipv6_rib_routes;
  #endif /* HAVE_IPV6 */

  netlink_route_read(&info);

  struct route_ipv4 * route;
  LIST_FOR_EACH(route, struct route_ipv4, node, &ipv4_rib_routes)
  {
    // print route
    char prefix_str[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &(route->p->prefix.s_addr), prefix_str, INET_ADDRSTRLEN) != 1)
  	printf("%s/%d [%u/%u]\n", prefix_str, route->p->prefixlen, route->distance, route->metric);
  }

  return 0;
}

int api_add_all_ports(struct list * list)
{
  // set up callbacks
  struct netlink_port_info info;
  memset(&info, 0, sizeof(info));
  info.add_port = iface_add_port;
  info.all_ports = list;
  netlink_link_read(&info);

  struct sw_port * port;
  LIST_FOR_EACH(port, struct sw_port, node, list)
  {
    printf("Interface %d: %s\n", port->port_no, port->hw_name);
  }
  return 0;
}

/* Add an IPv4 Address to RIB. */
int rib_add_ipv4 (struct route_ipv4 * route, struct list * list)
{
  list_push_back(list, &route->node);

  return 0;
}

#ifdef HAVE_IPV6
int rib_add_ipv6 (struct route_ipv6 * route, void * data)
{
//	struct list ** rib = &ipv6_rib_routes;
//	if (data != NULL)
//		rib = (struct list **)data;
	
//	struct listnode * node = malloc(sizeof(struct listnode));
//	if (*rib != NULL && node != NULL)
//	{
//		node->data = (void *)route;
//		LIST_APPEND((*rib), node);
//	}
	
	return 0;
}
#endif /* HAVE_IPV6 */


int iface_add_port (struct sw_port * port, struct list * list)
{
  list_push_back(list, &port->node);

  return 0;
}
