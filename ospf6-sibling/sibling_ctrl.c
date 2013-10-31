#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <netinet/in.h>

#include "util.h"
#include "debug.h"
#include "dblist.h"
#include "prefix.h"
#include "rfpbuf.h"
#include "routeflow-common.h"
#include "thread.h"
#include "prefix.h"
#include "if.h"
#include "ospf6_interface.h"
#include "ospf6_replica.h"
#include "ctrl_client.h"
#include "sibling_ctrl.h"

struct ctrl_client * ctrl_client = NULL;

int recv_features_reply(struct ctrl_client * ctrl_client, struct rfpbuf * buffer)
{
  struct rfp_router_features * rrf = buffer->data;
  struct interface * ifp;
  int i;
  unsigned int ifindex;
  unsigned int mtu;
  int offset = offsetof(struct rfp_router_features, ports);
  size_t n_ports = ((ntohs(rrf->header.length)
                                     - offset)
                        / sizeof(*rrf->ports));
  if(IS_OSPF6_SIBLING_DEBUG_MSG)
  {
    zlog_notice("number of ports: %d", n_ports);
  }
  for(i = 0; i < n_ports; i++)
  {  
    const struct rfp_phy_port * rpp = &rrf->ports[i];
    ifindex = ntohs(rpp->port_no);
    mtu = ntohl(rpp->mtu);
    if(IS_OSPF6_SIBLING_DEBUG_MSG)
    {
      zlog_notice("port #: %d, name: %s, mtu: %d", ifindex, rpp->name, mtu);
    }
    /* create new interface if not created */
    ifp = if_get_by_name(rpp->name);

    // fill up the interface info
    ifp->ifindex = ifindex;

    // fill up the mtu
    ifp->mtu = mtu;
    // copy over the flags
    ifp->state = ntohl(rpp->state);
    ospf6_interface_if_add(ifp, ctrl_client);
  }

  ctrl_client_if_addr_req(ctrl_client);

  return 0;
}

int if_address_add_v4(struct ctrl_client * ctrl_client, struct rfpbuf * buffer)
{
  const struct rfp_ipv4_address * address = buffer->data;
  struct interface * ifp = if_lookup_by_index(address->ifindex);
  struct connected * ifc;

  struct prefix p;

  memcpy(&p.u.prefix, &address->p, 4);
  p.prefixlen = address->prefixlen;
  p.family = AF_INET;

  ifc = connected_add_by_prefix(ifp, &p, NULL);
  if(ifc == NULL)
    return 1;

//  struct connected * ifc = calloc(1, sizeof(struct connected));
//  ifc->address = calloc(1, sizeof(struct prefix));
//  memcpy(&ifc->address->u.prefix, &address->p, 4); 
//  ifc->address->prefixlen = address->prefixlen;
//  ifc->address->family = AF_INET;

//  list_init(&ifc->node);

  if(IS_OSPF6_SIBLING_DEBUG_MSG)
  {
    char prefix_str[INET_ADDRSTRLEN];
    if(inet_ntop(AF_INET, &(p.u.prefix4.s_addr), prefix_str, INET_ADDRSTRLEN) != 1)
    {   
      zlog_debug("v4 addr: %s/%d", prefix_str, ifc->address->prefixlen);
    }   
  }

  // add addresss to list of connected 
//  list_push_back(&ifp->connected, &ifc->node);
//  ifc->ifp = ifp;

  return 0;
}

int if_address_add_v6(struct ctrl_client * ctrl_client, struct rfpbuf * buffer)
{
  const struct rfp_ipv6_address * address = buffer->data;
  struct interface * ifp = if_lookup_by_index(address->ifindex);
  struct connected * ifc;

  struct prefix p;

  memcpy(&p.u.prefix, &address->p, 16);
  p.prefixlen = address->prefixlen;
  p.family = AF_INET6;

  ifc = connected_add_by_prefix(ifp, &p, NULL);
  if(ifc == NULL)
    return 1;

//  struct connected * ifc = calloc(1, sizeof(struct connected));
//  ifc->address = calloc(1, sizeof(struct prefix));
//  memcpy(&ifc->address->u.prefix, &address->p, 16); 
//  ifc->address->prefixlen = address->prefixlen;
//  ifc->address->family = AF_INET6;

//  list_init(&ifc->node);

  if(IS_OSPF6_SIBLING_DEBUG_MSG)
  {
    char prefix_str[INET6_ADDRSTRLEN];
    if(inet_ntop(AF_INET6, &(p.u.prefix6.s6_addr), prefix_str, INET6_ADDRSTRLEN) != 1)
    {   
      zlog_debug("v6 addr: %s/%d", prefix_str, ifc->address->prefixlen);
    }   
  }

  // add addresss to list of connected 
//  list_push_back(&ifp->connected, &ifc->node);
//  ifc->ifp = ifp;

  // since connected address is AF_INET6, needs to be updated
  ospf6_interface_connected_route_update(ifc->ifp);

  return 0;
}

int recv_routes_reply(struct ctrl_client * ctrl_client, struct rfpbuf * buffer)
{
  const struct rfp_ipv4_route * rir = buffer->data;
  struct route_ipv4 * route = new_route();
 
  route->p = prefix_ipv4_new();
  route->p->family = AF_INET;
  memcpy(&route->p->prefix, &rir->p, 4);
  route->p->prefixlen = ntohs(rir->prefixlen);

  // print route
  char prefix_str[INET_ADDRSTRLEN];
  if (inet_ntop(AF_INET, &(route->p->prefix.s_addr), prefix_str, INET_ADDRSTRLEN) != 1)
    printf("%s/%d\n", prefix_str, route->p->prefixlen);

  return 0;
}

void sibling_ctrl_init(struct in6_addr * ctrl_addr, 
                       struct in6_addr * sibling_addr)
{
  ctrl_client = ctrl_client_new();
  ctrl_client_init(ctrl_client, ctrl_addr, sibling_addr);
  ctrl_client->features_reply = recv_features_reply;
  ctrl_client->routes_reply = recv_routes_reply;
  ctrl_client->leader_elect = ospf6_leader_elect;
  ctrl_client->address_add_v4 = if_address_add_v4;
  ctrl_client->address_add_v6 = if_address_add_v6;
}

void sibling_ctrl_interface_init(char * interface)
{
  ctrl_client_interface_init(ctrl_client, interface);
}
