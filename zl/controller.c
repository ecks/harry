#include "config.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>

#include "util.h"
#include "dblist.h"
#include "rfpbuf.h"
#include "rfp-msgs.h"
#include "routeflow-common.h"
#include "prefix.h"
#include "thread.h"
#include "rconn.h"
#include "datapath.h"
#include "controller.h"

/* The origin of a received OpenFlow message, to enable sending a reply. */
struct sender {
  struct conn *conn;      /* The device that sent the message. */
  uint32_t xid;           /* The OpenFlow transaction ID. */
};

int fwd_control_input(struct conn * conn, const struct sender *,
                      struct rfpbuf *);
static int send_routeflow_buffer(struct rfpbuf *buffer,
                     const struct sender *sender);
static int conn_recv(struct thread * t);
static void conn_destroy(struct conn *);

struct conn *
conn_create(struct datapath *dp, struct rconn * rconn)
{
  struct conn * conn;
  
  conn = calloc(1, sizeof *conn);
  conn->dp = dp;
  list_push_back(&dp->all_conns, &conn->node);
  conn->rconn = rconn;
  return conn;
}

void conn_start(struct conn * conn, const char * target)
{
  rconn_connect(conn->rconn, target);
}

void conn_run(struct conn * conn)
{
  rconn_run(conn->rconn);
}

void conn_wait(struct conn * conn)
{
  rconn_recv_wait(conn->rconn, conn_recv, conn);
}

static int conn_recv(struct thread * t)
{
  struct rfpbuf * buffer;
  struct rfp_header * rh;
  struct conn * conn = THREAD_ARG(t);

  // reregister ourselves
  conn_wait(conn);

  buffer = rconn_recv(conn->rconn);

  if (buffer->size >= sizeof *rh) 
  {
    struct sender sender;

    rh = (struct rfp_header *)buffer->data;
    sender.conn = conn;
    sender.xid = rh->xid;
    fwd_control_input(conn, &sender, buffer);
  } 
  else 
  {
    printf("received too-short OpenFlow message\n");
  }
  rfpbuf_delete(buffer);

  if(!rconn_is_alive(conn->rconn))
  {
    conn_destroy(conn);
  }
}

void conn_forward_msg(struct conn * conn, struct rfpbuf * buf)
{
  struct sender sender;
  struct rfp_header * rh = (struct rfp_header *)buf->data;

  sender.conn = conn;
  sender.xid = rh->xid;

  send_routeflow_buffer(buf, &sender);
}

static void
conn_destroy(struct conn *r)
{
  if (r) 
  {
    list_remove(&r->node);
    rconn_destroy(r->rconn);
    free(r);
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

static int
send_routeflow_buffer_to_remote(struct rfpbuf *buffer, struct conn *conn)
{
    int retval = rconn_send(conn->rconn, buffer);

    if (retval) {
        printf("send to %s failed: %s",
                     rconn_get_target(conn->rconn), strerror(retval));
    }
    return retval;
}

static int send_routeflow_buffer(struct rfpbuf *buffer,
                     const struct sender *sender)
{
    rfpmsg_update_length(buffer);
    if (sender) 
    { 
        /* Send back to the sender. */
        return send_routeflow_buffer_to_remote(buffer, sender->conn);
    }
    else
    {
        return 0;
    }
}

static void
fill_port_desc(struct sw_port * p, struct rfp_phy_port * desc)
{
  desc->port_no = htons(p->port_no);
  strncpy((char *) desc->name, p->hw_name, sizeof desc->name);
  desc->state = htonl(p->state);
  desc->mtu = htonl(p->mtu);
}

static void
send_features_reply(struct conn * conn, const struct sender *sender)
{
    struct rfpbuf *buffer;
    struct rfp_router_features *rfr;
    struct sw_port * p;

    buffer = make_openflow_reply(RFPT_FEATURES_REPLY, RFP10_VERSION,
                               sizeof(struct rfp_router_features), sender);
    
    rfr = buffer->l2;
    rfr->datapath_id  = htonl(conn->dp->id);
    LIST_FOR_EACH(p, struct sw_port, node, &conn->dp->port_list)
    {
      
      struct rfp_phy_port * rpp = rfpbuf_put_uninit(buffer, sizeof(struct rfp_phy_port));
      memset(rpp, 0, sizeof *rpp);
      fill_port_desc(p, rpp);
    }
    send_routeflow_buffer(buffer, sender);
}

static int recv_features_request(struct conn * conn, const struct sender *sender,
                      struct rfpbuf * msg)
{
    send_features_reply(conn, sender);
    return 0;
}

static void
send_addresses_reply(struct conn * conn, const struct sender * sender)
{
  struct rfpbuf *buffer;
  struct sw_port * p;

  buffer = make_openflow_reply(RFPT_ADDRESSES_REPLY, RFP10_VERSION,
                               sizeof(struct rfp_router_addresses), sender);
    
 LIST_FOR_EACH(p, struct sw_port, node, &conn->dp->port_list)
 {
   struct addr * addr;
   LIST_FOR_EACH(addr, struct addr, node, &p->connected)
   {
     if(addr->p->family == AF_INET)
     {
       struct rfp_connected_v4 * connected = rfpbuf_put_uninit(buffer, sizeof(struct rfp_connected_v4));
       connected->type= addr->p->family;
       connected->prefixlen = addr->p->prefixlen;

       memcpy(&connected->p, &addr->p->u.prefix, 4);

       connected->ifindex = p->port_no;
     }
     else if(addr->p->family == AF_INET6)
     {
       struct rfp_connected_v6 * connected = rfpbuf_put_uninit(buffer, sizeof(struct rfp_connected_v6));
       connected->type= addr->p->family;
       connected->prefixlen = addr->p->prefixlen;

       memcpy(&connected->p[0], &addr->p->u.prefix, 16);

       connected->ifindex = p->port_no;
     }
   }
 }

 send_routeflow_buffer(buffer, sender);
}

static int recv_addresses_request(struct conn * conn, const struct sender *sender,
                      struct rfpbuf * msg)
{
    send_addresses_reply(conn, sender);
    return 0;
}

static void fill_route_desc(struct route_ipv4 * route, struct rfp_route * desc)
{
  desc->type = htons(route->type);
  desc->flags = htons(route->flags);
  desc->prefixlen = htons(route->p->prefixlen);
  memcpy(&desc->p, &route->p->prefix, 4);
  desc->metric = htons(route->metric);
  desc->distance = htonl(route->distance);
  desc->table = htonl(route->vrf_id);
}

static void send_stats_routes_reply(struct conn * conn, const struct sender * sender)
{
  struct rfpbuf * buffer;
  struct rfp_stats_routes *rsr;
  struct route_ipv4 * route;

  buffer = make_openflow_reply(RFPT_STATS_ROUTES_REPLY, RFP10_VERSION,
                                sizeof(struct rfp_stats_routes), sender);

  rsr = buffer->l2;
 
  LIST_FOR_EACH(route, struct route_ipv4, node, &conn->dp->ipv4_rib_routes)
  {
    struct rfp_route * rr = rfpbuf_put_uninit(buffer, sizeof(struct rfp_route));
    memset(rr, 0, sizeof *rr);
    fill_route_desc(route, rr);
  }
 
  send_routeflow_buffer(buffer, sender);
}

static int
recv_stats_routes_request(struct conn * conn, const struct sender * sender,
                          struct rfpbuf * msg)
{
  send_stats_routes_reply(conn, sender);
  return 0;
}

static int
forward_ospf6_msg(struct conn * conn, const struct sender * sender, struct rfpbuf * msg)
{
  dp_forward_zl_to_punt(conn->dp, msg);
  return 0;
}

/* 'msg', which is 'length' bytes long, was received from the control path.
 * Apply it to 'chain'. */
int
fwd_control_input(struct conn * conn, const struct sender *sender, struct rfpbuf * buf)
{
    int (*handler)(struct conn *, const struct sender *, struct rfpbuf *);
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

      case RFPT_ADDRESSES_REQUEST:
        printf("Receiving addresses request message\n");
        handler = recv_addresses_request;
        break;

      case RFPT_STATS_ROUTES_REQUEST:
        printf("Receiving stats routes request message\n");
        handler = recv_stats_routes_request;
        break;

      case RFPT_FORWARD_OSPF6:
        printf("Forwarding OSPF6 traffic with length %d and xid %d\n", ntohs(rh->length), ntohl(rh->xid));
        handler = forward_ospf6_msg;
        break;

      default:
        return -EINVAL;
    }

    /* Handle it. */
    return handler(conn, sender, buf);
} 
