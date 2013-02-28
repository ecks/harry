#include "stdlib.h"
#include "stdint.h"
#include "stdio.h"
#include "stdbool.h"
#include "stddef.h"
#include "string.h"
#include "netinet/in.h"

#include "rconn.h"
#include "dblist.h"
#include "rfpbuf.h"
#include "rfp-msgs.h"
#include "routeflow-common.h"
#include "prefix.h"
#include "table.h"
#include "rib.h"
#include "router.h"
#include "sib_router.h"

struct router ** routers = NULL;
int * n_routers_p;

//static void sib_router_send_features_request(struct sib_router *);
//static int sib_router_process_features(struct sib_router * rt, void *rh);
//static void sib_router_process_phy_port(struct sib_router * rt, void * rpp_);
static void sib_router_send_features_reply();
static void sib_router_redistribute(struct sib_router * sr);
static void sib_router_forward_ospf6(struct sib_router *, struct rfpbuf *);

/* Creates and returns a new learning switch whose configuration is given by
 * 'cfg'.
 * 'rconn' is used to send out an OpenFlow features request. */
struct sib_router *
sib_router_create(struct rconn *rconn, struct router ** my_routers, int * my_n_routers_p)
{
  struct sib_router * rt;
  size_t i;

  rt = malloc(sizeof *rt);
  rt->rconn = rconn;
  rt->state = S_CONNECTING;

//  for(i = 0; i < MAX_PORTS; i++)
//  {
//    rt->port_states[i] = P_DISABLED;
//  }

  routers = my_routers;
  n_routers_p = my_n_routers_p;

  return rt;
}

//static void
//sib_router_handshake(struct sib_router * rt)
//{
//  sib_router_send_features_request(rt);
//}

void sib_router_run(struct sib_router * rt)
{
  int i;

  rconn_run(rt->rconn);

  if(rt->state == S_CONNECTING)
  {
//    if(rconn_get_version(rt->rconn) != -1)
//    {
//      sib_router_handshake(rt);
      rt->state = S_FEATURES_REPLY;
//    }
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

    sib_router_process_packet(rt, msg);
    rfpbuf_delete(msg);
  }
}

void
sib_router_wait(struct sib_router * rt)
{
  rconn_run_wait(rt->rconn);
  rconn_recv_wait(rt->rconn);
}

void
sib_router_process_packet(struct sib_router * sr, struct rfpbuf * msg)
{
  enum rfptype type;
  struct rfp_header * rh;

  rh = msg->data;

  switch(rh->type)
  {
    case RFPT_FEATURES_REPLY:
      if(sr->state == S_FEATURES_REPLY)
      {
        printf("features reply\n");
//        if(!sib_router_process_features(rt, msg->data))
//        {
//          rt->state = S_ROUTING;
//        }
//        else
//        {
//          rconn_disconnect(rt->rconn);
//        }
      }
      break;

    case RFPT_FEATURES_REQUEST:
      printf("features request\n");
      sib_router_send_features_reply(sr);   
      break;

    case RFPT_REDISTRIBUTE_REQUEST:
      printf("redistribute request\n");
      sib_router_redistribute(sr);
      break;

    case RFPT_FORWARD_OSPF6:
      printf("forward ospf6 packet\n");
      sib_router_forward_ospf6(sr, msg);
      break;

    default:
      printf("unknown packet\n");
  }
}

bool
sib_router_is_alive(const struct sib_router *rt)
{
    return rconn_is_alive(rt->rconn);
}

/* Destroys 'sw'. */
void
sib_router_destroy(struct sib_router *sw)
{
  if(sw)
  {
    rconn_destroy(sw->rconn);
    free(sw);
  } 
}

static void
sib_router_send_features_reply(struct sib_router * sr)
{
  if(routers == NULL)
    printf("router is empty\n");
  else
  {
    printf("router is full\n");

    struct rfpbuf * buffer;
    struct rfp_router_features * rfr;
    int i;
    int j;
    int retval;

    buffer = routeflow_alloc_xid(RFPT_FEATURES_REPLY, RFP10_VERSION, 0, sizeof(struct rfp_router_features));
  
    rfr = buffer->l2;

    for(i = 0; i < *n_routers_p; i++) // dereference the pointer
    {
      for(j = 0; j < MAX_PORTS; j++)
      {
        if(routers[i]->port_states[j] == P_FORWARDING)
        {
          struct rfp_phy_port * rpp = rfpbuf_put_uninit(buffer, sizeof(struct rfp_phy_port));
          memset(rpp, 0, sizeof *rpp);
          rpp->port_no = htons(j);
          rpp->state = htonl(RFPPS_FORWARD);
        }
      }

      rfpmsg_update_length(buffer);
      retval = rconn_send(sr->rconn, buffer);
  
      if (retval) {
          printf("send to %s failed: %s",
                       rconn_get_target(sr->rconn), strerror(retval));
      }
    }
  } 
}


static void
sib_router_send_route_reply(enum rfp_type type, struct sib_router * sr, struct prefix * p, struct rib * rib)
{
  struct rfpbuf * buffer;
  struct rfp_ipv4_route * rir;
  int retval;

  buffer = routeflow_alloc_xid(type, RFP10_VERSION, 0, sizeof(struct rfp_ipv4_route));

  rir = buffer->l2;

  rir->prefixlen = htons(p->prefixlen);
  memcpy(&rir->p, &p->u.prefix, 4);

  rfpmsg_update_length(buffer);
  retval = rconn_send(sr->rconn, buffer);

  if(retval)
  {
    printf("send to %s failed: %s",
           rconn_get_target(sr->rconn), strerror(retval));
  }
}

static void
sib_router_redistribute(struct sib_router * sr)
{
  struct rib * newrib;
  struct route_table * table;
  struct route_node * rn;

  table = vrf_table(AFI_IP, SAFI_UNICAST, 0);
  if (table)
    for (rn = route_top (table); rn; rn = route_next (rn))
      for (newrib = rn->info; newrib; newrib = newrib->next)
        if(newrib->distance != DISTANCE_INFINITY)
//            && zebra_check_addr (&rn->p)) // TODO: implement function
          sib_router_send_route_reply (RFPT_IPV4_ROUTE_ADD, sr, &rn->p, newrib);
}

static void
sib_router_forward_ospf6(struct sib_router * sr, struct rfpbuf * msg)
{
  int retval = rconn_send(sr->rconn, msg);
  if(retval)
  {
    printf("send to %s failed: %s\n",
    		rconn_get_target(sr->rconn), strerror(retval));
  }
}

/*
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
  unsigned int *port_state = &rt->port_states[port_no];
  unsigned int new_port_state;

  printf("Port number: %d, name: %s", port_no, rpp->name);
  
  switch(state)
  {
    case RFPPS_LINK_DOWN:
      printf(" at down state\n");
      new_port_state = P_DISABLED;
      break;

    case RFPPS_FORWARD:
      printf(" at forwarding state\n");
      new_port_state = P_FORWARDING;
      break;

    default:
      break;
  } 

  if(*port_state != new_port_state)
  {
    *port_state = new_port_state;
  }
}
*/
