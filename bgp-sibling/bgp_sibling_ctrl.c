#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include <netinet/in.h>

#include "util.h"
#include "dblist.h"
#include "routeflow-common.h"
#include "if.h"
#include "rfpbuf.h"
#include "prefix.h"
#include "bgp_ctrl_client.h"
#include "bgp_sibling_ctrl.h"

static struct bgp_ctrl_client * bgp_ctrl_client = NULL;

int recv_features_reply(struct bgp_ctrl_client * bgp_ctrl_client, struct rfpbuf * buffer)
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
  }
  
  return 0;
}

int recv_routes_reply(struct bgp_ctrl_client * bgp_ctrl_client, struct rfpbuf * buffer)
{
  const struct rfp_ipv4_route * rir = buffer->data;
  struct route_ipv4 * route = new_route();

  route->p = new_prefix_v4();
  route->p->family = AF_INET;
  memcpy(&route->p->prefix, &rir->p, 4);
  route->p->prefixlen = ntohs(rir->prefixlen);

  // print route
  char prefix_str[INET_ADDRSTRLEN];
  if(inet_ntop(AF_INET, &(route->p->prefix.s_addr), prefix_str, INET_ADDRSTRLEN) != 1)
    printf("%s/%d\n", prefix_str, route->p->prefixlen);

  return 0;
}

void bgp_sibling_ctrl_init(struct in6_addr * addr)
{
  bgp_ctrl_client = bgp_ctrl_client_new();
  bgp_ctrl_client_init(bgp_ctrl_client, addr);
  bgp_ctrl_client->features_reply = recv_features_reply;
  bgp_ctrl_client->routes_reply = recv_routes_reply;
}
