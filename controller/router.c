#include "config.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <netinet/in.h>

#include "util.h"
#include "routeflow-common.h"
#include "dblist.h"
#include "prefix.h"
#include "thread.h"
#include "rconn.h"
#include "dblist.h"
#include "rfp-msgs.h"
#include "rfpbuf.h"
#include "rconn.h"
#include "rib.h"
#include "debug.h"
#include "if.h"
#include "sib_router.h"
#include "router.h"

static void router_send_features_request(struct router *);
static void router_send_stats_routes_request(struct router *rt);
static void router_process_packet(struct router *, struct rfpbuf *);
static int router_process_addresses(struct router * rt, void * rh);
static int router_process_features(struct router * rt, void *rh);
static void router_process_phy_port(struct router * rt, void * rpp_);
static void router_send_stats_routes_request(struct router *rt);
static int router_process_stats_routes(struct router * rt, void * rh);
static void router_process_route(struct router * rt, void * rr_);

/* Creates and returns a new learning switch whose configuration is given by
 * 'cfg'.
 * 'rconn' is used to send out an OpenFlow features request. */
struct router *
router_create(struct rconn *rconn)
{
  struct router * rt;
  size_t i;

  rt = malloc(sizeof *rt);
  rt->rconn = rconn;
  rt->state = R_CONNECTING;

  list_init(&rt->port_list);

  rt->iflist = if_init();

  return rt;
}

static void
router_handshake(struct router * rt)
{
  printf("Router handshake...\n");
  // find out port info
  router_send_features_request(rt);

  // find out route info
  router_send_stats_routes_request(rt);
}

int router_run(struct thread * t)
{
  struct router * rt = THREAD_ARG(t);
  int i;

  rconn_run(rt->rconn);

  switch(rt->state)
  {
    case R_CONNECTING:
      printf("Outside of router_run ==> R_CONNECTING\n");
      break;

    case R_CONNECTED:
      printf("Outside of router_run ==> R_CONNECTED\n");

    default:
      break;
  }

  if(rt->state == R_CONNECTING)
  {
//    if(rconn_get_version(rt->rconn) != -1)
//    {
    if(rconn_exchanged_hellos(rt->rconn))
    {   
      router_handshake(rt);
      rt->state = R_CONNECTED;
    }
//    }
    router_wait(rt);
    return;
  }

  for(i = 0; i < 50; i++)
  {
    struct rfpbuf * msg;
    
    msg = rconn_recv(rt->rconn);
    if(!msg)
    {
      break;
    }

    router_process_packet(rt, msg);
    rfpbuf_delete(msg);
  }

  if(router_is_alive(rt))
  {
    router_wait(rt);
  }
  else
  {
    router_destroy(rt);
  }

  return 0;
}

void
router_wait(struct router * rt)
{
//  rconn_run_wait(rt->rconn);
  rconn_recv_wait(rt->rconn, router_run, rt);
}

/* The order of features reply and stats reply are not important
 * What is important is that both of these messages are received so 
 * that we can move to a state of routing
 */
static void
router_process_packet(struct router * rt, struct rfpbuf * msg)
{
  enum rfptype type;
  struct rfp_header * rh;
  int i;
  int xid;

  rh = msg->data;

  switch(rh->type)
  {
    case RFPT_FEATURES_REPLY:
      if(rt->state == R_CONNECTED || rt->state == R_STATS_ROUTES_REPLY)
      {
        printf("features reply\n");
        if(!router_process_features(rt, msg->data))
        {
          if(rt->state == R_CONNECTED)
          {
            rt->state = R_FEATURES_REPLY;
          }
          else if(rt->state == R_STATS_ROUTES_REPLY)
          {
            rt->state = R_FEAT_STATS_REPLY;
          }
        }
        else
        {
          rconn_disconnect(rt->rconn);
        }
      }
      break;

    case RFPT_ADDRESSES_REPLY:
      if(rt->state == R_FEATURES_REPLY || rt->state == R_FEAT_STATS_REPLY)
      {
        printf("addresses reply\n");
        if(!router_process_addresses(rt, msg->data))
        {
          if(rt->state == R_FEATURES_REPLY)
          {
            rt->state = R_FEAT_ADDR_REPLY;
          }
          else if(rt->state == R_FEAT_STATS_REPLY)
          {
            rt->state = R_ROUTING;
          }
        }
      }
      break;

    case RFPT_STATS_ROUTES_REPLY:
      if(rt->state == R_CONNECTED || rt->state == R_FEATURES_REPLY || rt->state == R_FEAT_ADDR_REPLY)
      {
        printf("stats reply\n");
        if(!router_process_stats_routes(rt, msg->data))
        {
          if(rt->state == R_CONNECTED)
          {
            rt->state = R_STATS_ROUTES_REPLY;
          }
          else if(rt->state == R_FEATURES_REPLY)
          {
            rt->state = R_FEAT_STATS_REPLY;
          }
          else if(rt->state == R_FEAT_ADDR_REPLY)
          {
            rt->state = R_ROUTING;
          }
        }
      }
      break;

    case RFPT_FORWARD_OSPF6:
      if(rt->state == R_ROUTING)
      {
        // try to get a unique xid
        xid = etcd_get_unique_xid();

        if(IS_CONTROLLER_DEBUG_MSG)
        {
          // forward to all siblings
//          zlog_debug("forward ospf6 packet: controller => sibling, xid: %d", ntohl(rh->xid));
          zlog_debug("forward ospf6 packet: controller => sibling, xid: %d", xid);
        }
        struct rfpbuf * msg_copy = rfpbuf_clone(msg);

        // need to modify xid here
        rh = msg_copy->data;
        rh->xid = htonl(xid);

        sib_router_forward_ospf6(msg_copy, xid);

        // sent the data, no longer needed
        rfpbuf_delete(msg_copy);
      }
      break;

    case RFPT_FORWARD_BGP:
      if(rt->state == R_ROUTING)
      {
        // forward to all siblings
        printf("forward bgp packet: controller => sibling\n");
        struct rfpbuf * msg_copy = rfpbuf_clone(msg);
        sib_router_forward_bgp(msg_copy);
      }
      break;

    default:
      printf("unknown packet\n");
      break;
  }
}

bool
router_is_alive(const struct router *rt)
{
    return rconn_is_alive(rt->rconn);
}

/* Destroys 'sw'. */
void
router_destroy(struct router *rt)
{
  if(rt)
  {
    rconn_destroy(rt->rconn);
    free(rt);
  } 
}

static void
router_send_addresses_request(struct router *rt)
{
  struct rfpbuf * buffer;
 
  buffer = routeflow_alloc(RFPT_ADDRESSES_REQUEST, RFP10_VERSION, sizeof(struct rfp_header));

  rconn_send(rt->rconn, buffer);
}

static int 
router_process_addresses(struct router * rt, void * rh)
{
  struct rfp_router_addresses * rra = rh;
  u_char * p;

  struct rfp_connected * connected = rra->connected;
  p = (u_char *)((void *)rh + sizeof(struct rfp_header));

  do
  {
    if(connected->type == AF_INET)
    {
      struct rfp_connected_v4 * connected4 = (struct rfp_connected_v4 *)connected;
      printf("v4 received\n");
      
      struct connected * ifc = calloc(1, sizeof(struct connected));
      ifc->address = calloc(1, sizeof(struct prefix));
      memcpy(&ifc->address->u.prefix, &connected4->p, 4);
      ifc->address->prefixlen = connected4->prefixlen;
      ifc->address->family = AF_INET;

      list_init(&ifc->node);
      char prefix_str[INET_ADDRSTRLEN];
      if(inet_ntop(AF_INET, &(ifc->address->u.prefix4.s_addr), prefix_str, INET_ADDRSTRLEN) != 1)
      {
        printf("v4 addr: %s/%d\n", prefix_str, connected4->prefixlen);
      }

      struct interface * ifp = if_lookup_by_index(rt->iflist, connected4->ifindex);
      
      // add addresss to list of connected 
      list_push_back(&ifp->connected, &ifc->node);
      ifc->ifp = ifp;

      p += sizeof(struct rfp_connected_v4);
      connected = (void *)connected + sizeof(struct rfp_connected_v4);
    }
    else if(connected->type == AF_INET6)
    {
      struct rfp_connected_v6 * connected6 = (struct rfp_connected_v6 *)connected;
      printf("v6 received\n");

      struct connected * ifc = calloc(1, sizeof(struct connected));
      ifc->address = calloc(1, sizeof(struct prefix));
      memcpy(&ifc->address->u.prefix, &connected6->p, 16);
      ifc->address->prefixlen = connected6->prefixlen;
      ifc->address->family = AF_INET6;

      char prefix_str[INET6_ADDRSTRLEN];
      if(inet_ntop(AF_INET6, &(ifc->address->u.prefix6.s6_addr), prefix_str, INET6_ADDRSTRLEN) != 1)
      {
        printf("v6 addr: %s/%d\n", prefix_str, connected6->prefixlen);
      }

      struct interface * ifp = if_lookup_by_index(rt->iflist, connected6->ifindex);
      
      // add addresss to list of connected 
      list_push_back(&ifp->connected, &ifc->node);
      ifc->ifp = ifp;

      p += sizeof(struct rfp_connected_v6);
      connected = (void *)connected + sizeof(struct rfp_connected_v6);
     }
  }
  while(p < ((void *)rh + ntohs(((struct rfp_header *)rh)->length)));

  return 0;
}

static void
router_send_features_request(struct router *rt)
{
  struct rfpbuf * buffer;
 
  buffer = routeflow_alloc(RFPT_FEATURES_REQUEST, RFP10_VERSION, sizeof(struct rfp_header));

  rconn_send(rt->rconn, buffer);
}

static int
router_process_features(struct router * rt, void * rh)
{
  struct rfp_router_features *rrf = rh;
  int offset = offsetof(struct rfp_router_features, ports);
  size_t n_ports = ((ntohs(rrf->header.length)
                    - offset)
                    / sizeof(*rrf->ports));
  size_t i;

  printf("Number of ports: %d\n", n_ports);
 
  for(i = 0; i < n_ports; i++)
  {
    router_process_phy_port(rt, &rrf->ports[i]);
  }

  router_send_addresses_request(rt);

  return 0;
}

static void
router_process_phy_port(struct router * rt, void * rpp_)
{
  const struct rfp_phy_port * rpp = rpp_;
  uint16_t port_no = ntohs(rpp->port_no);
  uint32_t state = ntohl(rpp->state);
  unsigned int mtu = ntohl(rpp->mtu);
  struct iflist_ * iflist_;

  struct interface * ifp = if_get_by_name(rt->iflist, rpp->name);

  ifp->ifindex = port_no;
  ifp->state = state;
  ifp->mtu = mtu;

  iflist_ = calloc(1, sizeof(struct iflist_));
  list_init(&iflist_->node);

  // link to ifp and back
  ifp->info = iflist_;
  iflist_->ifp = ifp;
  list_push_back(&rt->port_list, &iflist_->node);

  printf("Port number: %d, name: %s, mtu: %d", port_no, rpp->name, mtu);
  
  switch(state)
  {
    case RFPPS_LINK_DOWN:
      printf(" at down state\n");
      break;

    case RFPPS_FORWARD:
      printf(" at forwarding state\n");
      break;

    default:
      break;
  } 

}

static void
router_send_stats_routes_request(struct router *rt)
{
  struct rfpbuf * buffer;

  buffer = routeflow_alloc(RFPT_STATS_ROUTES_REQUEST, RFP10_VERSION, sizeof(struct rfp_header));

  rconn_send(rt->rconn, buffer);
}

static int
router_process_stats_routes(struct router * rt, void * rh)
{
  struct rfp_stats_routes * rsr = rh;
  int offset = offsetof(struct rfp_stats_routes, routes);
  size_t n_routes = ((ntohs(rsr->header.length)
                     - offset)
                     / sizeof(*rsr->routes));
  size_t i;

  printf("Number of routes: %d\n", n_routes);
  
  for(i = 0; i < n_routes; i++)
  {
    router_process_route(rt, &rsr->routes[i]);
  }
  return 0;
}

static void
router_process_route(struct router * rt, void * rr_)
{
  const struct rfp_route * rr = rr_;
  struct route_ipv4 * route = new_route();
  unsigned int distance;
  unsigned long int metric;

  route->p = prefix_ipv4_new();
  route->p->family = AF_INET;
  memcpy(&route->p->prefix, &rr->p, 4);
  route->p->prefixlen = ntohs(rr->prefixlen);
  route->metric = ntohs(rr->metric);
  route->distance = ntohl(rr->distance);
  route->vrf_id = ntohl(rr->table);

  // print route
  char prefix_str[INET_ADDRSTRLEN];
  if (inet_ntop(AF_INET, &(route->p->prefix.s_addr), prefix_str, INET_ADDRSTRLEN) != 1)
    printf("%s/%d [%u/%u]\n", prefix_str, route->p->prefixlen, route->distance, route->metric);

  rib_add_ipv4(route, SAFI_UNICAST);

}

void router_forward(struct router * r, struct rfpbuf * msg)
{
  int retval = rconn_send(r->rconn, msg);
  if(retval)
  {
    printf("send to %s failed: %s\n",
    		rconn_get_target(r->rconn), strerror(retval));
  }
}
