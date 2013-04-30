#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <netinet/in.h>

#include "dblist.h"
#include "rfpbuf.h"
#include "routeflow-common.h"
#include "thread.h"
#include "prefix.h"
#include "if.h"
#include "ospf6_interface.h"
#include "ctrl_client.h"
#include "sibling_ctrl.h"

struct ctrl_client * ctrl_client = NULL;

int recv_features_reply(struct ctrl_client * ctrl_client, struct rfpbuf * buffer)
{
  struct rfp_router_features * rrf = buffer->data;
  struct interface * ifp;
  int i;
  unsigned int ifindex;
  int offset = offsetof(struct rfp_router_features, ports);
  size_t n_ports = ((ntohs(rrf->header.length)
                                     - offset)
                        / sizeof(*rrf->ports));
  printf("number of ports: %d\n", n_ports);
  for(i = 0; i < n_ports; i++)
  {  
    const struct rfp_phy_port * rpp = &rrf->ports[i];
    ifindex = ntohs(rpp->port_no);
    printf("port #: %d, name: %s\n", ifindex, rpp->name);

    /* create new interface if not created */
    ifp = if_get_by_name(rpp->name);

    // fill up the interface info
    ifp->ifindex = ifindex;

    // copy over the flags
    ifp->state = ntohl(rpp->state);
    ospf6_interface_if_add(ifp, ctrl_client);
  }
}

int recv_routes_reply(struct ctrl_client * ctrl_client, struct rfpbuf * buffer)
{
  const struct rfp_ipv4_route * rir = buffer->data;
  struct route_ipv4 * route = new_route();
 
  route->p = new_prefix_v4();
  route->p->family = AF_INET;
  memcpy(&route->p->prefix, &rir->p, 4);
  route->p->prefixlen = ntohs(rir->prefixlen);

  // print route
  char prefix_str[INET_ADDRSTRLEN];
  if (inet_ntop(AF_INET, &(route->p->prefix.s_addr), prefix_str, INET_ADDRSTRLEN) != 1)
    printf("%s/%d\n", prefix_str, route->p->prefixlen);
}

void sibling_ctrl_init(struct in6_addr * ctrl_addr)
{
  ctrl_client = ctrl_client_new();
  ctrl_client_init(ctrl_client, ctrl_addr);
  ctrl_client->features_reply = recv_features_reply;
  ctrl_client->routes_reply = recv_routes_reply;
}