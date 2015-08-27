#include "config.h"

#include "stdio.h"
#include "stdlib.h"
#include "stdbool.h"
#include "stdint.h"
#include "stddef.h"
#include "errno.h"
#include "string.h"
#include "netinet/in.h"
#include "linux/if.h"

#include "dblist.h"
#include "prefix.h"
#include "routeflow-common.h"
#include "thread.h"
#include "util.h"
#include "byte-order.h"
#include "dblist.h"
#include "rfpbuf.h"
#include "rfp-msgs.h"
#include "rconn.h"
#include "vconn.h"
#include "debug.h"
#include "controller.h"
#include "datapath.h"

/* The origin of a received OpenFlow message, to enable sending a reply. */
struct sender {
    struct rfconn *rfconn;      /* The device that sent the message. */
    uint32_t xid;               /* The OpenFlow transaction ID. */
};

int fwd_control_input(struct datapath *, const struct sender *,
                      struct rfpbuf *);
static int forward_ospf6_msg(struct datapath * dp, const struct sender * sender, struct rfpbuf * msg);
void get_ports(struct list * ports);
void get_addrs(struct list * ports);
void get_routes(struct list * ipv4_rib_routes, struct list * ipv6_rib_routes);
static void rfconn_run(struct datapath *, struct rfconn *);
static void rfconn_forward_msg(struct datapath * dp, struct rfconn * rfconn, struct rfpbuf * buf);
static void rfconn_destroy(struct rfconn *);

struct datapath * dp_new(uint64_t dpid)
{
    struct datapath *dp;

    dp = calloc(1, sizeof *dp);
    if (!dp) {
        return NULL;
    }

    list_init(&dp->all_conns);

    list_init(&dp->port_list);
    get_ports(&dp->port_list);
    get_addrs(&dp->port_list);

    list_init(&dp->ipv4_rib_routes);
#ifdef HAVE_IPV6
    list_init(&dp->ipv6_rib_routes);
    get_routes(&dp->ipv4_rib_routes, &dp->ipv6_rib_routes);
#else
    get_routes(&dp->ipv4_rib_routes, NULL);
#endif

    return dp;
}

void
dp_run(struct datapath *dp)
{
  struct conn *conn, *next_conn;

  /* Talk ot remote controllers */
  LIST_FOR_EACH_SAFE(conn, next_conn, struct conn, node, &dp->all_conns)
  {
    conn_run(conn);
    conn_wait(conn);
  }
}

void dp_forward_zl_to_ctrl(struct datapath * dp, struct rfpbuf * buf)
{
  struct conn *conn, *next_conn;

  /* Talk ot remote controllers */
  LIST_FOR_EACH_SAFE(conn, next_conn, struct conn, node, &dp->all_conns)
  {
    conn_forward_msg(conn, buf);
  }
}

void dp_forward_zl_to_punt(struct datapath * dp, struct rfpbuf * buf)
{
  zl_serv_forward(buf);
}

/* Creates a new controller for 'target' in 'mgr'.  update_controller() needs
 * to be called later to finish the new ofconn's configuration. */
void
add_controller(struct datapath *dp, const char *target)
{
  struct conn * conn;

  conn = conn_create(dp, rconn_create());
  conn_start(conn, target);
}

struct sw_port * iface_find_port(unsigned int index, struct list * ports)
{
  struct sw_port * port;
  LIST_FOR_EACH(port, struct sw_port, node, ports)
  {
    if(port->port_no == index)
      return port;
  }

  return NULL;
}

int iface_add_port (unsigned int index, unsigned int flags, unsigned int mtu, char * name, struct list * list)
{
  struct sw_port * port = malloc(sizeof *port);
  if(port != NULL)
  {
    port->port_no =  index;
    port->mtu = mtu;
    if(flags & IFF_LOWER_UP && !(flags & IFF_DORMANT))
    {
      port->state = RFPPS_FORWARD;
    }
    else
    {
      port->state = RFPPS_LINK_DOWN;
    }
    strncpy(port->hw_name, name, strlen(name));
    list_push_back(list, &port->node);

    list_init(&port->connected);
  } 

  return 0;
}

void
get_ports(struct list * ports)
{
  if(interface_list(ports, iface_add_port) != 0)
  {
    exit(1);
  }

  if(IS_ZEBRALITE_DEBUG_RIB)
  {
    struct sw_port * port;
    LIST_FOR_EACH(port, struct sw_port, node, ports)
    {
      zlog_debug("Interface %d: %s", port->port_no, port->hw_name);
      if(port->state == RFPPS_FORWARD)
      {
        zlog_debug("Link is up!");
      }
      else
      {
        zlog_debug("Link is down!");
      }
    }
  } 
}

int addr_add_ipv4(int index, void * address, u_char prefixlen, struct list * list)
{
  struct sw_port * port;
  struct addr * addr = calloc(1, sizeof(struct addr));
  addr->p = calloc(1, sizeof(struct prefix_ipv4));

  addr->p->family = AF_INET;
  memcpy(&addr->p->u.prefix, address, 4);
  addr->p->prefixlen = prefixlen;

  addr->ifindex = index;

  char prefix_str[INET_ADDRSTRLEN];
  if(inet_ntop(AF_INET, &(addr->p->u.prefix4.s_addr), prefix_str, INET_ADDRSTRLEN) != 1)
  {
    printf("addr_add_ipv4: %s/%d if: %d\n", prefix_str, prefixlen, index);
  }

  port = iface_find_port(index, list);
  if(port)
  {
    list_push_back(&port->connected, &addr->node);
  }

  return 0;
}

int addr_add_ipv6(int index, void * address, u_char prefixlen, struct list * list)
{
  struct sw_port * port;
  struct addr * addr = calloc(1, sizeof(struct addr));
  addr->p = calloc(1, sizeof(struct prefix_ipv6));

  addr->p->family = AF_INET6;
  memcpy(&addr->p->u.prefix, address, 16);
  addr->p->prefixlen = prefixlen;

  addr->ifindex = index;

  char prefix_str[INET6_ADDRSTRLEN];
  if(inet_ntop(AF_INET6, &(addr->p->u.prefix6.s6_addr), prefix_str, INET6_ADDRSTRLEN) != 1)
  {
    printf("addr_add_ipv6: %s/%d if: %d\n", prefix_str, prefixlen, index);
  }

  port = iface_find_port(index, list);
  if(port)
  {
    list_push_back(&port->connected, &addr->node);
  }

  return 0;
}

void get_addrs(struct list * ports)
{
  // ports is going to be for both ipv4 and ipv6 addresses
  if(addrs_list(ports, ports, addr_add_ipv4, addr_add_ipv6) != 0)
  {
    exit(1);
  }

}

/* Add an IPv4 Address to RIB. */
int rib_add_ipv4 (struct route_ipv4 * route, void * data)
{
  struct list * list = (struct list *)data;
  list_push_back(list, &route->node);

  return 0;
}

#ifdef HAVE_IPV6
int rib_add_ipv6 (struct route_ipv6 * route, void * data)
{
  struct list * list = (struct list *)data;
  list_push_back(list, &route->node);
  return 0;
}
#endif /* HAVE_IPV6 */

void
get_routes(struct list * ipv4_rib_routes, struct list * ipv6_rib_routes)
{
  if(routes_list(ipv4_rib_routes, ipv6_rib_routes, rib_add_ipv4, rib_add_ipv6) != 0)
  {
    exit(1);
  }

  if(IS_ZEBRALITE_DEBUG_RIB)
  {
    struct route_ipv4 * route4;
    LIST_FOR_EACH(route4, struct route_ipv4, node, ipv4_rib_routes)
    {
      // print ipv4 route
      char prefix_str[INET_ADDRSTRLEN];
      if (inet_ntop(AF_INET, &(route4->p->prefix.s_addr), prefix_str, INET_ADDRSTRLEN) != 1)
      	zlog_debug("%s/%d [%u/%u] (%d)", prefix_str, route4->p->prefixlen, route4->distance, route4->metric, route4->ifindex);
    }

    struct route_ipv6 * route6;
    LIST_FOR_EACH(route6, struct route_ipv6, node, ipv6_rib_routes)
    {
      // print ipv6 route
      char prefix_str[INET6_ADDRSTRLEN];
      if(inet_ntop(AF_INET6, &(route6->p->prefix.s6_addr), prefix_str, INET6_ADDRSTRLEN) != 1)
      	zlog_debug("%s/%d [%u/%u] (%d)", prefix_str, route6->p->prefixlen, route6->distance, route6->metric, route6->ifindex);
    }
  }
}

void 
set_route_v6(struct route_ipv6 * route, struct in6_addr * nexthop_addr, struct list * ipv6_rib_routes)
{
  struct route_ipv6 * rib_route;

  if(IS_ZEBRALITE_DEBUG_RIB)
  {
    char prefix_str[INET6_ADDRSTRLEN];
    if(inet_ntop(AF_INET6, &(route->p->prefix.s6_addr), prefix_str, INET6_ADDRSTRLEN) != 1)
      zlog_debug("Adding route to kernel rib: %s/%d", prefix_str, route->p->prefixlen);
    char nexthop_str[INET6_ADDRSTRLEN];
    if(inet_ntop(AF_INET6, nexthop_addr, nexthop_str, INET6_ADDRSTRLEN) != 1)
      zlog_debug("Nexthop addr is %s", nexthop_str);
  }

  LIST_FOR_EACH(rib_route, struct route_ipv6, node, ipv6_rib_routes)
  {
    if((memcmp(&(route->p->prefix), &(rib_route->p->prefix), sizeof(struct in6_addr)) == 0) && 
       (route->p->prefixlen == rib_route->p->prefixlen))
    {
//      if(IS_ZEBRALITE_DEBUG_RIB)
//        zlog_debug("Found matching rib already in RIB, no need to add it to FIB");

      // the routes are set, so free memory
//      free(route->p);
//      free(route);

      // implicitly delete the old route from the kernel fib and software rib
      unset_route_v6(rib_route);

      list_remove(&rib_route->node);
    }
  }


  list_init(&route->node);
  list_push_back(ipv6_rib_routes, &route->node);

  install_route_v6(route, nexthop_addr);
}

void
unset_route_v6(struct route_ipv6 * route)
{
  uninstall_route_v6(route);
}

/*
static void
rfconn_run(struct datapath * dp, struct rfconn *r)
{
  int i;

  rconn_run(r->rconn);

  for(i = 0; i < 2; i++)
  {
    struct rfpbuf * buffer;
    struct rfp_header * rh;

    buffer = rconn_recv(r->rconn);
    if(!buffer)
    {
      break;
    }

    if (buffer->size >= sizeof *rh) 
    {
      struct sender sender;

      rh = (struct rfp_header *)buffer->data;
      sender.rfconn = r;
      sender.xid = rh->xid;
      fwd_control_input(dp, &sender, buffer);
    } 
    else 
    {
      printf("received too-short OpenFlow message\n");
    }
    rfpbuf_delete(buffer);
  }

  if(!rconn_is_alive(r->rconn))
  {
    rfconn_destroy(r);
  }
}


static void
rfconn_forward_msg(struct datapath * dp, struct rfconn * rfconn, struct rfpbuf * buf)
{
  struct sender sender;
  struct rfp_header * rh;

  rh = (struct rfp_header *)buf->data;

  sender.rfconn = rfconn;
  sender.xid = rh->xid;
  forward_ospf6_msg(dp, &sender, buf);

}

static void
rfconn_destroy(struct rfconn *r)
{
    if (r) 
    {
        list_remove(&r->node);
        rconn_destroy(r->rconn);
        free(r);
    }
}

static int
send_routeflow_buffer_to_remote(struct rfpbuf *buffer, struct rfconn *rfconn)
{
    int retval = rconn_send(rfconn->rconn, buffer);

    if (retval) {
        printf("send to %s failed: %s",
                     rconn_get_target(rfconn->rconn), strerror(retval));
    }
    return retval;
} */
/*
static int
send_routeflow_buffer(struct datapath *dp, struct rfpbuf *buffer,
                     const struct sender *sender)
{
    rfpmsg_update_length(buffer);
    if (sender) 
    { */
        /* Send back to the sender. */ /*
        return send_routeflow_buffer_to_remote(buffer, sender->rfconn);
    }
    else
    {
        return 0;
    }
} */

/*
struct rfpbuf *
make_openflow_reply(uint8_t type, uint8_t version, size_t openflow_len,
                    const struct sender *sender)
{
  struct rfpbuf * rfpbuf;
  rfpbuf = routeflow_alloc_xid(type, version, sender ? sender->xid : 0,
                             openflow_len);
  return rfpbuf;
}

static void
fill_port_desc(struct sw_port * p, struct rfp_phy_port * desc)
{
  desc->port_no = htons(p->port_no);
  strncpy((char *) desc->name, p->hw_name, sizeof desc->name);
  desc->state = htonl(p->state);
}

static void
dp_send_features_reply(struct datapath *dp, const struct sender *sender)
{
    struct rfpbuf *buffer;
    struct rfp_router_features *rfr;
    struct sw_port * p;

    buffer = make_openflow_reply(RFPT_FEATURES_REPLY, RFP10_VERSION,
                               sizeof(struct rfp_router_features), sender);
    
    rfr = buffer->l2;
    rfr->datapath_id  = htonll(dp->id);
    LIST_FOR_EACH(p, struct sw_port, node, &dp->port_list)
    {
      
      struct rfp_phy_port * rpp = rfpbuf_put_uninit(buffer, sizeof(struct rfp_phy_port));
      memset(rpp, 0, sizeof *rpp);
      fill_port_desc(p, rpp);
    }
    send_routeflow_buffer(dp, buffer, sender);
}

static int
recv_features_request(struct datapath *dp, const struct sender *sender,
                      struct rfpbuf * msg)
{
    dp_send_features_reply(dp, sender);
    return 0;
}

static void
fill_route_desc(struct route_ipv4 * route, struct rfp_route * desc)
{
  desc->type = htons(route->type);
  desc->flags = htons(route->flags);
  desc->prefixlen = htons(route->p->prefixlen);
  memcpy(&desc->p, &route->p->prefix, 4);
  desc->metric = htons(route->metric);
  desc->distance = htonl(route->distance);
  desc->table = htonl(route->vrf_id);
}

static void
dp_send_stats_routes_reply(struct datapath * dp, const struct sender * sender)
{
  struct rfpbuf * buffer;
  struct rfp_stats_routes *rsr;
  struct route_ipv4 * route;

  buffer = make_openflow_reply(RFPT_STATS_ROUTES_REPLY, RFP10_VERSION,
                                sizeof(struct rfp_stats_routes), sender);

  rsr = buffer->l2;
 
  LIST_FOR_EACH(route, struct route_ipv4, node, &dp->ipv4_rib_routes)
  {
    struct rfp_route * rr = rfpbuf_put_uninit(buffer, sizeof(struct rfp_route));
    memset(rr, 0, sizeof *rr);
    fill_route_desc(route, rr);
  }
 
  send_routeflow_buffer(dp, buffer, sender);
}

static int
recv_stats_routes_request(struct datapath *dp, const struct sender * sender,
                          struct rfpbuf * msg)
{
  dp_send_stats_routes_reply(dp, sender);
  return 0;
}

static int
forward_ospf6_msg(struct datapath * dp, const struct sender * sender, struct rfpbuf * msg)
{
  send_routeflow_buffer(dp, msg, sender);
  return 0;
}
*/
/* 'msg', which is 'length' bytes long, was received from the control path.
 * Apply it to 'chain'. */
/*
int
fwd_control_input(struct datapath *dp, const struct sender *sender,
                  struct rfpbuf * buf)
{
    int (*handler)(struct datapath *, const struct sender *, struct rfpbuf *);
    struct rfp_header * rh;
*/
    /* Check encapsulated length. */
/*    rh = (struct rfp_header *) buf->data; */


/* Not sure why this is needed for now */
//    if (ntohs(rh->length) > length) {
//        return -EINVAL;
//    }

    /* Figure out how to handle it. */
/*    switch (rh->type) 
    {
      case RFPT_FEATURES_REQUEST:
        printf("Receiving features request message\n");
        handler = recv_features_request;
        break;

      case RFPT_STATS_ROUTES_REQUEST:
        printf("Receiving stats routes request message\n");
        handler = recv_stats_routes_request;
        break;

      case RFPT_FORWARD_OSPF6:
        printf("Forwarding OSPF6 traffic\n");
//        handler = forward_ospf6_msg;
        break;

      default:
        return -EINVAL;
    }
*/
    /* Handle it. */
/*    return handler(dp, sender, buf);
} */
