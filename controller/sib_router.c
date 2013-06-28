#include "string.h"
#include "stdlib.h"
#include "stdint.h"
#include "stdio.h"
#include "stdbool.h"
#include "stddef.h"
#include "string.h"
#include "errno.h"
#include "netinet/in.h"

#include "util.h"
#include "socket-util.h"
#include "dblist.h"
#include "routeflow-common.h"
#include "paxosflow-common.h"
#include "sisis_process_types.h"
#include "thread.h"
#include "rconn.h"
#include "if.h"
#include "rfpbuf.h"
#include "vconn-provider.h"
#include "rfp-msgs.h"
#include "prefix.h"
#include "table.h"
#include "rib.h"
#include "router.h"
#include "sib_router.h"

#include "libpaxos.h"
#include "ctrl_paxos.h"

#define MAX_OSPF6_SIBLINGS 3
#define MAX_BGP_SIBLINGS 3

#define MAX_LISTENERS 16

extern struct thread_master * master;

struct router ** routers = NULL;
int * n_routers_p;

static struct sib_router * ospf6_siblings[MAX_OSPF6_SIBLINGS];
int n_ospf6_siblings;

static struct sib_router * bgp_siblings[MAX_BGP_SIBLINGS];
int n_bgp_siblings;

static int sib_listener_accept(struct thread * t);
static void new_sib_router(struct sib_router ** sr, struct vconn *vconn, const char *name);

//static void sib_router_send_features_request(struct sib_router *);
//static int sib_router_process_features(struct sib_router * rt, void *rh);
//static void sib_router_process_phy_port(struct sib_router * rt, void * rpp_);
static void sib_router_send_features_reply();
static void sib_router_redistribute(struct sib_router * sr);

void sib_router_init(struct router ** my_routers, int * my_n_routers_p)
{
  int retval;
  int i;
  int n_sib_listeners;
  struct pvconn * sib_listeners[MAX_LISTENERS];
  struct pvconn * sib_ospf6_pvconn;  // sibling channel interface
  struct pvconn * sib_bgp_pvconn;    // sibling channel interface
  struct list * dst_ptype_lst;

  dst_ptype_lst = list_new();
  list_init(dst_ptype_lst);

  struct dst_ptype * bgp_dst_ptype = calloc(1, sizeof(struct dst_ptype));
  bgp_dst_ptype->ptype = SISIS_PTYPE_BGP_SBLING;
  list_push_back(dst_ptype_lst, &bgp_dst_ptype->node);

  struct dst_ptype * ospf6_dst_ptype = calloc(1, sizeof(struct dst_ptype));
  ospf6_dst_ptype->ptype = SISIS_PTYPE_OSPF6_SBLING;
  list_push_back(dst_ptype_lst, &ospf6_dst_ptype->node);

  ctrl_paxos_new(SISIS_PTYPE_CTRL, dst_ptype_lst);

  if(learner_init(ctrl_paxos_deliver, ctrl_paxos_init) != 0)
  {
    printf("Failed to start the learner!\n");
    exit(1); 
  }

  n_ospf6_siblings = 0;
  n_bgp_siblings = 0;

  n_sib_listeners = 0;

  retval = pvconn_open("ptcp6:6634", &sib_ospf6_pvconn, DSCP_DEFAULT);
  if(!retval)
  {
    if(n_sib_listeners >= MAX_LISTENERS)
    {
      printf("max switch %d connections\n", n_sib_listeners);
    }

    pvconn_wait(sib_ospf6_pvconn, sib_listener_accept, sib_ospf6_pvconn);

    sib_listeners[n_sib_listeners++] = sib_ospf6_pvconn;
  }

  retval = pvconn_open("ptcp6:6635", &sib_bgp_pvconn, DSCP_DEFAULT);
  if(!retval)
  {
    if(n_sib_listeners >= MAX_LISTENERS)
    {
      printf("max switch %d connections\n", n_sib_listeners);
    }

    pvconn_wait(sib_bgp_pvconn, sib_listener_accept, sib_bgp_pvconn);

    sib_listeners[n_sib_listeners++] = sib_bgp_pvconn;
  }
 
  // copy over pointers to routers
  routers = my_routers;
  n_routers_p = my_n_routers_p;
}

static int sib_listener_accept(struct thread * t)
{
  struct pvconn * sib_listener = THREAD_ARG(t);
  struct vconn * new_sib_vconn;
  int retval;

  retval = pvconn_accept(sib_listener, RFP10_VERSION, &new_sib_vconn);
  if(!retval)
  {
    // TODO: expect only one router connection for now
//    if(*n_routers_p == 1)
//    {
      if(strcmp(sib_listener->name, "ptcp6:6634") == 0)
      {
        // ospf6 connection
        printf("new ospf6 sibling connection\n");
        new_sib_router(&ospf6_siblings[n_ospf6_siblings++], new_sib_vconn, "tcp6");
      }
      if(strcmp(sib_listener->name, "ptcp6:6635") == 0)
      {
        // bgp connection
        printf("new bgp sibling connection\n");
        new_sib_router(&bgp_siblings[n_bgp_siblings++], new_sib_vconn, "tcp6");
      }
//    }
//    else
//    {
//      exit(1);
//    }
 }
  else if(retval != EAGAIN)
  {
    pvconn_close(sib_listener);
  }

  // reregister ourselves
  pvconn_wait(sib_listener, sib_listener_accept, sib_listener);

  return 0;
}

static void new_sib_router(struct sib_router ** sr, struct vconn *vconn, const char *name)
{
    struct rconn * rconn;
  
    rconn = rconn_create();
    rconn_connect_unreliably(rconn, vconn, NULL);

    // expect to have only one interface for now
//    if(*n_routers_p == 1)  // dereference the pointer
//    {
      *sr = sib_router_create(rconn);

      // schedule an event to call sib_router_run
      thread_add_event(master, sib_router_run, *sr, 0);
//    }
//    else
//    {
//      exit(1);  // error condition
//    }
}

/* Creates and returns a new learning switch whose configuration is given by
 * 'cfg'.
 * 'rconn' is used to send out an OpenFlow features request. */
struct sib_router *
sib_router_create(struct rconn *rconn)
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

  return rt;
}

//static void
//sib_router_handshake(struct sib_router * rt)
//{
//  sib_router_send_features_request(rt);
//}

int sib_router_run(struct thread * t)
{
  struct sib_router * rt = THREAD_ARG(t);
  int i;

  rconn_run(rt->rconn);

  if(rt->state == S_CONNECTING)
  {
//    if(rconn_get_version(rt->rconn) != -1)
//    {
//      sib_router_handshake(rt);
      rt->state = S_FEATURES_REPLY;
//    }
    sib_router_wait(rt);
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

  if(sib_router_is_alive(rt))
  {
    sib_router_wait(rt);
  }
  else
  {
    sib_router_destroy(rt);
  }

  return 0;
}

void
sib_router_wait(struct sib_router * rt)
{
//  rconn_run_wait(rt->rconn);
  rconn_recv_wait(rt->rconn, sib_router_run, rt);
}

void
sib_router_process_packet(struct sib_router * sr, struct rfpbuf * msg)
{
  enum rfptype type;
  struct rfp_header * rh;
  int i;

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
      for(i = 0; i < *n_routers_p; i++)
      {
        printf("forward ospf6 packet: sibling => controller\n");
        struct rfpbuf * msg_copy = rfpbuf_clone(msg);
        router_forward(routers[i], msg_copy);
      }
      break;

    case RFPT_FORWARD_BGP:
      for(i = 0; i < *n_routers_p; i++)
      {
        printf("forward bgp packet: sibling => controller\n");
        struct rfpbuf * msg_copy = rfpbuf_clone(msg);
        router_forward(routers[i], msg_copy);
      }
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
      struct if_list * if_node;
      LIST_FOR_EACH(if_node, struct if_list, node, &routers[i]->port_list)
      {
        struct interface * ifp = if_node->ifp;
        struct rfp_phy_port * rpp = rfpbuf_put_uninit(buffer, sizeof(struct rfp_phy_port));
        memset(rpp, 0, sizeof *rpp);
        rpp->port_no = htons(ifp->ifindex);
        rpp->state = htonl(ifp->state);
        strncpy(rpp->name, ifp->name, strlen(ifp->name));
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

void sib_router_forward_ospf6(struct rfpbuf * msg)
{
  int i;
  int retval;

  for(i = 0; i < n_ospf6_siblings; i++)
  {
    retval = rconn_send(ospf6_siblings[i]->rconn, msg);
    if(retval)
    {
      printf("send to %s failed: %s\n",
    		  rconn_get_target(ospf6_siblings[i]->rconn), strerror(retval));
    }
  }
}

void sib_router_forward_bgp(struct rfpbuf * msg)
{
  int i;
  int retval;

  for(i = 0; i < n_bgp_siblings; i++)
  {
    retval = rconn_send(bgp_siblings[i]->rconn, msg);
    if(retval)
    {
      printf("send to %s failed: %s\n",
    		  rconn_get_target(ospf6_siblings[i]->rconn), strerror(retval));
    }
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
