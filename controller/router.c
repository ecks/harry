#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <netinet/in.h>

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
#include "if.h"
#include "sib_router.h"
#include "router.h"

struct sib_router ** sib_routers = NULL;
int * n_siblings_p;

static void router_send_features_request(struct router *);
static void router_send_stats_routes_request(struct router *rt);
static void router_process_packet(struct router *, struct rfpbuf *);
static int router_process_features(struct router * rt, void *rh);
static void router_process_phy_port(struct router * rt, void * rpp_);
static void router_send_stats_routes_request(struct router *rt);
static int router_process_stats_routes(struct router * rt, void * rh);
static void router_process_route(struct router * rt, void * rr_);

/* Creates and returns a new learning switch whose configuration is given by
 * 'cfg'.
 * 'rconn' is used to send out an OpenFlow features request. */
struct router *
router_create(struct rconn *rconn, struct sib_router ** my_sib_routers, int * my_n_siblings_p)
{
  struct router * rt;
  size_t i;

  rt = malloc(sizeof *rt);
  rt->rconn = rconn;
  rt->state = R_CONNECTING;

  sib_routers = my_sib_routers;
  n_siblings_p = my_n_siblings_p;

  list_init(&rt->port_list);
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
            rt->state = R_ROUTING;
          }
        }
        else
        {
          rconn_disconnect(rt->rconn);
        }
      }
      break;

    case RFPT_STATS_ROUTES_REPLY:
      if(rt->state == R_CONNECTED || rt->state == R_FEATURES_REPLY)
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
            rt->state = R_ROUTING;
          }
        }
      }
      break;

    case RFPT_FORWARD_OSPF6:
      if(rt->state == R_ROUTING)
      {
        // forward to all siblings
        for(i = 0; i < *n_siblings_p; i++) // dereference the pointer
        {
          printf("forward ospf6 packet: controller => sibling\n");
          struct rfpbuf * msg_copy = rfpbuf_clone(msg);
          sib_router_forward_ospf6(sib_routers[i], msg_copy);
        }
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
  return 0;
}

static void
router_process_phy_port(struct router * rt, void * rpp_)
{
  const struct rfp_phy_port * rpp = rpp_;
  uint16_t port_no = ntohs(rpp->port_no);
  uint32_t state = ntohl(rpp->state);
  struct if_list * if_list;

  struct interface * ifp = if_get_by_name(rpp->name);

  ifp->ifindex = port_no;
  ifp->state = state;

  if_list = calloc(1, sizeof(struct if_list));
  list_init(&if_list->node);

  // link to ifp and back
  ifp->info = if_list;
  if_list->ifp = ifp;
  list_push_back(&rt->port_list, &if_list->node);

  printf("Port number: %d, name: %s", port_no, rpp->name);
  
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

  route->p = new_prefix_v4();
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

void router_forward_ospf6(struct router * r, struct rfpbuf * msg)
{
  int retval = rconn_send(r->rconn, msg);
  if(retval)
  {
    printf("send to %s failed: %s\n",
    		rconn_get_target(r->rconn), strerror(retval));
  }
}
