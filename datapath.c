#include "config.h"

#include "stdio.h"
#include "stdlib.h"
#include "stdbool.h"
#include "stdint.h"
#include "stddef.h"
#include "errno.h"
#include "string.h"
#include "arpa/inet.h"
#include "linux/if.h"

#include "lib/dblist.h"
#include "lib/prefix.h"
#include "lib/routeflow-common.h"
#include "lib/util.h"
#include "lib/byte-order.h"
#include "lib/dblist.h"
#include "lib/rfpbuf.h"
#include "lib/rfp-msgs.h"
#include "lib/rconn.h"
#include "lib/vconn.h"
#include "datapath.h"

/* The origin of a received OpenFlow message, to enable sending a reply. */
struct sender {
    struct rfconn *rfconn;      /* The device that sent the message. */
    uint32_t xid;               /* The OpenFlow transaction ID. */
};

/* A connection to a secure channel. */
struct rfconn {
    struct list node;
    struct datapath * dp;
    struct rconn *rconn;
};

int fwd_control_input(struct datapath *, const struct sender *,
                      struct rfpbuf *);
static int forward_ospf6_msg(struct datapath * dp, const struct sender * sender, struct rfpbuf * msg);
void get_ports(struct list * ports);
void get_routes(struct list * ipv4_rib_routes, struct list * ipv6_rib_routes);
static struct rfconn *rfconn_create(struct datapath *, struct rconn *);
static void rfconn_run(struct datapath *, struct rfconn *);
static void rfconn_forward_msg(struct datapath * dp, struct rfconn * rfconn, struct rfpbuf * buf);
static void rfconn_destroy(struct rfconn *);

int dp_new(struct datapath **dp_, uint64_t dpid)
{
    struct datapath *dp;

    dp = calloc(1, sizeof *dp);
    if (!dp) {
        return ENOMEM;
    }

    list_init(&dp->all_conns);

    list_init(&dp->port_list);
    get_ports(&dp->port_list);
 
    list_init(&dp->ipv4_rib_routes);
#ifdef HAVE_IPV6
    list_init(&dp->ipv6_rib_routes);
    get_routes(&dp->ipv4_rib_routes, &dp->ipv6_rib_routes);
#else
    get_routes(&dp->ipv4_rib_routes, NULL);
#endif

    *dp_ = dp;
    return 0;
}

void
dp_run(struct datapath *dp)
{
  struct rfconn *rfconn, *next_rfconn;

  /* Talk ot remote controllers */
  LIST_FOR_EACH_SAFE(rfconn, next_rfconn, struct rfconn, node, &dp->all_conns)
  {
    rfconn_run(dp, rfconn);
  }
}

void dp_forward_msg(struct datapath * dp, struct rfpbuf * buf)
{
  struct rfconn *rfconn, *next_rfconn;

  /* Talk ot remote controllers */
  LIST_FOR_EACH_SAFE(rfconn, next_rfconn, struct rfconn, node, &dp->all_conns)
  {
    rfconn_forward_msg(dp, rfconn, buf);
  }
}

/* Creates a new controller for 'target' in 'mgr'.  update_controller() needs
 * to be called later to finish the new ofconn's configuration. */
void
add_controller(struct datapath *dp, const char *target)
{
  struct rfconn * rfconn;

  rfconn = rfconn_create(dp, rconn_create());
  rconn_connect(rfconn->rconn, target);
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

  struct sw_port * port;
  LIST_FOR_EACH(port, struct sw_port, node, ports)
  {
    printf("Interface %d: %s\n", port->port_no, port->hw_name);
    if(port->state == RFPPS_FORWARD)
    {
      printf("Link is up!\n");
    }
    else
    {
      printf("Link is down!\n");
    }
  }
  
}

void
get_routes(struct list * ipv4_rib_routes, struct list * ipv6_rib_routes)
{
  if(routes_list(ipv4_rib_routes, ipv6_rib_routes) != 0)
  {
    exit(1);
  }
}

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

static struct rfconn *
rfconn_create(struct datapath *dp, struct rconn *rconn)
{
  struct rfconn * rfconn;
  
  rfconn = malloc(sizeof *rfconn);
  rfconn->dp = dp;
  list_push_back(&dp->all_conns, &rfconn->node);
  rfconn->rconn = rconn;

  return rfconn;
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
}

static int
send_routeflow_buffer(struct datapath *dp, struct rfpbuf *buffer,
                     const struct sender *sender)
{
    rfpmsg_update_length(buffer);
    if (sender) 
    {
        /* Send back to the sender. */
        return send_routeflow_buffer_to_remote(buffer, sender->rfconn);
    }
    else
    {
        return 0;
    }
}

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

/* 'msg', which is 'length' bytes long, was received from the control path.
 * Apply it to 'chain'. */
int
fwd_control_input(struct datapath *dp, const struct sender *sender,
                  struct rfpbuf * buf)
{
    int (*handler)(struct datapath *, const struct sender *, struct rfpbuf *);
    struct rfp_header * rh;

    /* Check encapsulated length. */
    rh = (struct rfp_header *) buf->data;


/* Not sure why this is needed for now */
//    if (ntohs(rh->length) > length) {
//        return -EINVAL;
//    }

    /* Figure out how to handle it. */
    switch (rh->type) 
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

    /* Handle it. */
    return handler(dp, sender, buf);
}
