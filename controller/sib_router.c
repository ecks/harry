#include "config.h"

#include "string.h"
#include "stdlib.h"
#include "stdint.h"
#include "stdio.h"
#include "stdbool.h"
#include "stddef.h"
#include "string.h"
#include "signal.h"
#include "errno.h"
#include "netinet/in.h"

#include "util.h"
#include "socket-util.h"
#include "routeflow-common.h"
#include "thread.h"
#include "rconn.h"
#include "dblist.h"
#include "if.h"
#include "rfpbuf.h"
#include "vconn-provider.h"
#include "rfp-msgs.h"
#include "prefix.h"
#include "table.h"
#include "sisis_api.h"
#include "rib.h"
#include "debug.h"
#include "thread.h"
#include "router.h"
#include "sib_router.h"

#define MAX_OSPF6_SIBLINGS 3
#define MAX_BGP_SIBLINGS 3

#define MAX_LISTENERS 16

extern struct thread_master * master;

struct router ** routers = NULL;
int * n_routers_p;

static struct sib_router * ospf6_siblings[MAX_OSPF6_SIBLINGS];
int n_ospf6_siblings;
int n_ospf6_siblings_routing; // number of ospf6 siblings that have transitioned to routing state

static struct sib_router * bgp_siblings[MAX_BGP_SIBLINGS];
int n_bgp_siblings;

static int sib_listener_accept(struct thread * t);
static void new_sib_router(struct sib_router ** sr, struct vconn *vconn, const char *name);
static void reuse_sib_router(struct sib_router * sr, struct vconn * vconn, const char * name);

static struct sib_router * sib_router_create(struct rconn *);
static void sib_router_reuse(struct sib_router *, struct rconn *);

//static void sib_router_send_features_request(struct sib_router *);
//static int sib_router_process_features(struct sib_router * rt, void *rh);
static void sib_router_process_phy_port(struct sib_router * rt, void * rpp_);
static void sib_router_send_features_reply(struct sib_router * sr, unsigned int xid);
static void sib_router_redistribute(struct sib_router * sr, unsigned int xid);
static void sib_router_if_addr_add(struct sib_router * sr, unsigned int xid);
static void sib_router_send_leader_elect();
static void state_transition(struct sib_router * sr, enum sib_router_state state);

void sib_router_init(struct router ** my_routers, int * my_n_routers_p)
{
  int retval;
  int i;
  int n_sib_listeners;
  struct pvconn * sib_listeners[MAX_LISTENERS];
  struct pvconn * sib_ospf6_pvconn;  // sibling channel interface
  struct pvconn * sib_bgp_pvconn;    // sibling channel interface

  n_ospf6_siblings = 0;

  n_bgp_siblings = 0;

  n_sib_listeners = 0;

  n_ospf6_siblings_routing = 0;

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
  int i;
  bool found_reusable_sib_router = false;

  retval = pvconn_accept(sib_listener, RFP10_VERSION, &new_sib_vconn);
  if(!retval)
  {
    // TODO: expect only one router connection for now
//    if(*n_routers_p == 1)
//    {
      if(strcmp(sib_listener->name, "ptcp6:6634") == 0)
      {
        // ospf6 connection
        if(CONTROLLER_DEBUG_MSG)
        {
          zlog_debug("new ospf6 sibling connection");
        }

        // try to find disconnected sib routers in order to try and reuse them
        for(i = 0; i < n_ospf6_siblings; i++)
        {
          if(ospf6_siblings[i]->state == SIB_DISCONNECTED)
          {
            if(CONTROLLER_DEBUG_MSG)
            {
              zlog_debug("found reusable sibling");
            }

            reuse_sib_router(ospf6_siblings[i], new_sib_vconn, "tcp6");
            found_reusable_sib_router = true;
            break;
          }
        }
        if(!found_reusable_sib_router)
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
  int retval;
  
  rconn = rconn_create();
  rconn_connect_unreliably(rconn, vconn, NULL);

  *sr = sib_router_create(rconn);

  // schedule an event to call sib_router_run
  thread_add_event(master, sib_router_run, *sr, 0);
}

static void reuse_sib_router(struct sib_router * sr, struct vconn * vconn, const char * name)
{
  struct rconn * rconn;
  int retval;
  
  rconn = rconn_create();
  rconn_connect_unreliably(rconn, vconn, NULL);

  sib_router_reuse(sr, rconn);

  // schedule an event to call sib_router_run
  thread_add_event(master, sib_router_run, sr, 0);
}

void sib_router_reuse(struct sib_router * sr, struct rconn * rconn)
{
  char * name = rconn->target;

  sr->name = malloc(strlen(name)); 
  strncpy(sr->name, name, strlen(name));

  sr->rconn = rconn;
  state_transition(sr, SIB_CONNECTING);
  list_init(&sr->msgs_rcvd_queue);

  sr->current_egress_xid = 0;

  sr->current_ingress_xid = 0;
  sr->ingress_ack_timeout = 5;

  // indicates that the timeout_ingress thread is inactive
  sr->thread_timeout_ingress = NULL;
 }

/* Creates and returns a new learning switch whose configuration is given by
 * 'cfg'.
 * 'rconn' is used to send out an OpenFlow features request. */
struct sib_router *
sib_router_create(struct rconn *rconn)
{
  struct sib_router * rt;
  size_t i;
  char * name = rconn->target;

  rt = malloc(sizeof *rt);
 
  rt->name = malloc(strlen(name)); 
  strncpy(rt->name, name, strlen(name));

  rt->rconn = rconn;
  state_transition(rt, SIB_CONNECTING);
  list_init(&rt->msgs_rcvd_queue);

  rt->current_egress_xid = 0;

  rt->current_ingress_xid = 0;
  rt->ingress_ack_timeout = 5;

  // indicates that the timeout_ingress thread is inactive
  rt->thread_timeout_ingress = NULL;
  return rt;
}

//static void
//sib_router_handshake(struct sib_router * rt)
//{
//  sib_router_send_features_request(rt);
//}

static bool 
majority_have_msg_rcvd()
{
  int i;
  for(i = 0; i < n_ospf6_siblings; i++)
  {
    if((ospf6_siblings[i]->state != SIB_DISCONNECTED) && (!list_empty(&ospf6_siblings[i]->msgs_rcvd_queue)))
    {
      i++;
    }
  }
  if(((float)i/OSPF6_NUM_SIBS) > 0.5)
  {
    return true;
  }

  return false;
}

static bool 
all_have_msg_rcvd()
{
  int i;
  for(i = 0; i < n_ospf6_siblings; i++)  
  {
    if(list_empty(&ospf6_siblings[i]->msgs_rcvd_queue))
      return false;
  }

  return true;
}

static void append_msg_to_rcvd_queue(struct sib_router * sr, struct rfpbuf * msg)
{
  struct rfpbuf * msg_rcvd = rfpbuf_clone(msg);
  list_init(&msg_rcvd->list_node);
  list_push_back(&sr->msgs_rcvd_queue, &msg_rcvd->list_node);
}

static void
vote_majority()
{
  if(CONTROLLER_DEBUG_MSG)
  {
    zlog_debug("inside vote majority");
  }

  struct rfp_header * rh;
  bool all_same = true;
  bool seen_before = false;
  int num_of_equal_msgs = 0;
  int i;
  int j;

  if(IS_CONTROLLER_DEBUG_MSG)
  { 
    for(i = 0; i < n_ospf6_siblings; i++)
    {
      if(list_empty(&ospf6_siblings[i]->msgs_rcvd_queue))
      {
        zlog_debug("at %d, list is empty", i);
      }
      else
      {
        rh = (rfpbuf_from_list(list_peek_front(&ospf6_siblings[i]->msgs_rcvd_queue)))->data;
        zlog_debug("at %d, current msg xid: %d", i, ntohl(rh->xid));
      }
    }
  }

  // the first message gets compared with the second message, second message
  // gets compared with third message, etc
  //
  // we need to make sure that a is an actual message
  j = 0;
  while(list_empty(&ospf6_siblings[j]->msgs_rcvd_queue) && (j < n_ospf6_siblings))
  {
    j++;
  }
  // a should now be an actual message
  struct rfpbuf * a = rfpbuf_from_list(list_peek_front(&ospf6_siblings[j]->msgs_rcvd_queue));

  for(i = j + 1; i < n_ospf6_siblings; i++)
  {
    if((ospf6_siblings[i]->state == SIB_DISCONNECTED) || (list_empty(&ospf6_siblings[i]->msgs_rcvd_queue)))
    {
      if(IS_CONTROLLER_DEBUG_MSG)
      {
        zlog_debug("sibling b is disconnected or has nothing in the queue");
      }
      seen_before = false;
    }
    else
    {
      struct rfpbuf * b = rfpbuf_from_list(list_peek_front(&ospf6_siblings[i]->msgs_rcvd_queue));

      rh = a->data;

      if(IS_CONTROLLER_DEBUG_MSG)
      {
        zlog_debug("at counter %d, a msg xid: %d", i, ntohl(rh->xid));
      }

      rh = b->data;

      if(IS_CONTROLLER_DEBUG_MSG)
      {
        zlog_debug("b msg xid: %d", ntohl(rh->xid));
      }

      if(!rfpbuf_equal(a, b))
      {
        struct ospf6_hello * a_hello = (struct ospf6_hello *)((void *)a->data + sizeof(struct rfp_header) + sizeof(struct ospf6_header));
        struct ospf6_hello * b_hello = (struct ospf6_hello *)((void *)b->data + sizeof(struct rfp_header) + sizeof(struct ospf6_header));

        zlog_debug("a and b not equal");
        seen_before = false;
      }
      else
      {
        if(!seen_before)
        {
          num_of_equal_msgs = num_of_equal_msgs + 2;
        }
        else
        {
          num_of_equal_msgs++;
        }
        seen_before = true;
        a = b;
      }
    }
  }

  if(CONTROLLER_DEBUG_MSG)
  {
    zlog_debug("num of equal msgs: %d", num_of_equal_msgs);
  }

  if(((float)num_of_equal_msgs/OSPF6_NUM_SIBS) > 0.5)
  {
    if(CONTROLLER_DEBUG_MSG)
    {
      zlog_debug("achieved majority");
    }
    // delete members from beginning of queue for the majority that matched
    for(i = 0; i < n_ospf6_siblings; i++)
    {
      if(!list_empty(&ospf6_siblings[i]->msgs_rcvd_queue))
      {
        struct rfpbuf * msg_to_delete = rfpbuf_from_list(list_pop_front(&ospf6_siblings[i]->msgs_rcvd_queue));
        if(msg_to_delete != a)
        {
          rfpbuf_delete(msg_to_delete);
        }
      }
    }
    for(i = 0; i < *n_routers_p; i++)
    {
      if(CONTROLLER_DEBUG_MSG)
      {
        struct rfp_header * rh = rfpbuf_at_assert(a, 0, a->size);
        zlog_debug("forward ospf6 packet: sibling => controller, xid %d", ntohl(rh->xid));
      }
      router_forward(routers[i], a);
    }

    // increase all the xid of all the siblings
    for(i = 0; i < n_ospf6_siblings; i++)
    {
      ospf6_siblings[i]->current_egress_xid++;
    }
  }  
}

int sib_router_run(struct thread * t)
{
  struct sib_router * rt = THREAD_ARG(t);
  int i;

  rconn_run(rt->rconn);

  if(rt->state == SIB_CONNECTING)
  {
//    if(rconn_get_version(rt->rconn) != -1)
//    {
//      sib_router_handshake(rt);
      state_transition(rt, SIB_CONNECTED);
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
  unsigned int xid;
  int i;

  rh = msg->data;
  xid = ntohl(rh->xid);

  switch(rh->type)
  {
    case RFPT_FEATURES_REQUEST:
      if(IS_CONTROLLER_DEBUG_MSG)
      {
        zlog_debug("features request, xid: %d from %s", xid, sr->rconn->target);
      }
      if(xid == sr->current_egress_xid && (sr->state == SIB_CONNECTED || sr->state == SIB_REDISTRIBUTE_REQ_RCVD))
      {
        sr->current_egress_xid++;
        sib_router_send_features_reply(sr, xid);   

        // state transition
        if(sr->state == SIB_CONNECTED)
        {
          state_transition(sr, SIB_FEATURES_REQ_RCVD);
        }
        else if(sr->state == SIB_REDISTRIBUTE_REQ_RCVD)
        {
          state_transition(sr, SIB_FEATURES_REDIS_REQ_RCVD);
        }
      }
      else
      {
        if(IS_CONTROLLER_DEBUG_MSG)
        {
          zlog_debug("features request, xid mismatch");
        }
      }
      break;

    case RFPT_IF_ADDRESS_REQ:
      if(IS_CONTROLLER_DEBUG_MSG)
      {
        zlog_debug("interface address add request, xid: %d from %s", xid, sr->rconn->target);
      }
      if(xid == sr->current_egress_xid && (sr->state == SIB_FEATURES_REQ_RCVD || SIB_FEATURES_REDIS_REQ_RCVD))
      {
        sr->current_egress_xid++;
        sib_router_if_addr_add(sr, xid);

        // state transititon
        if(sr->state == SIB_FEATURES_REQ_RCVD)
        {
          state_transition(sr, SIB_FEATURES_ADDR_REQ_RCVD);
        }
        else if(sr->state == SIB_FEATURES_REDIS_REQ_RCVD)
        {
          state_transition(sr, SIB_ROUTING);
        }
      } 
      else
      {
        if(IS_CONTROLLER_DEBUG_MSG)
        {
          zlog_debug("interface address request, xid mismatch");
        }
      }
      break;

    case RFPT_REDISTRIBUTE_REQUEST:
      if(IS_CONTROLLER_DEBUG_MSG)
      {
        zlog_debug("redistribute request, xid: %d from %s", xid, sr->rconn->target);
      }
      if(xid == sr->current_egress_xid && (sr->state == SIB_CONNECTED || sr->state == SIB_FEATURES_REQ_RCVD || sr->state == SIB_FEATURES_ADDR_REQ_RCVD))
      {
        sr->current_egress_xid++;
        sib_router_redistribute(sr, xid);

        // state transition
        if(sr->state == SIB_CONNECTED)
        {
          state_transition(sr, SIB_REDISTRIBUTE_REQ_RCVD);
        }
        else if(sr->state == SIB_FEATURES_REQ_RCVD)
        {
          state_transition(sr, SIB_FEATURES_REDIS_REQ_RCVD);
        }
        else if(sr->state == SIB_FEATURES_ADDR_REQ_RCVD)
        {
          state_transition(sr, SIB_ROUTING);
        }
      }
      else
      {
        if(IS_CONTROLLER_DEBUG_MSG)
        {
          zlog_debug("redistribute request, xid mismatch");
        }
      }
      break;

    case RFPT_IPV6_ROUTE_SET_REQUEST:
      if(IS_CONTROLLER_DEBUG_MSG)
      {
        zlog_debug("IPv6 Route Set Request, xid: %d from %s", xid, sr->rconn->target);
      }
      if(xid == sr->current_egress_xid)
      {
        append_msg_to_rcvd_queue(sr, msg);
        // first we check if all siblings are connected,
        // then we check if majority have received the messages
        if((n_ospf6_siblings == OSPF6_NUM_SIBS) &&
          (majority_have_msg_rcvd()))
        {
          // increment the current xid inside vote_majority
          vote_majority();
        }
      }
      else if(xid > sr->current_egress_xid)
      {
        // we need to append the message at the end of the list
        if(CONTROLLER_DEBUG_MSG)
        {
          zlog_debug("forward ospf6 packet: queued for later, xid %d from %s, current egress xid: %d", xid, sr->rconn->target, sr->current_egress_xid);
        }
        append_msg_to_rcvd_queue(sr, msg);
      }
      else
      {
        if(CONTROLLER_DEBUG_MSG)
        {
          zlog_debug("forward ospf6 packet: rejected, xid %d from %s, current egress xid: %d", xid, sr->rconn->target, sr->current_egress_xid);
        }
      }
      break;

    case RFPT_FORWARD_OSPF6:
      {
        if(xid == sr->current_egress_xid)
        {
          zlog_debug("forward ospf6 packet: xids %d matched from %s", xid, sr->rconn->target);

          struct ospf6_header * oh = (struct ospf6_header *)((void *)rh + sizeof(struct rfp_header));
          switch(oh->type)
          {
            case OSPF6_MESSAGE_TYPE_HELLO:
              zlog_debug("hello received");
              break; 

            case OSPF6_MESSAGE_TYPE_DBDESC:
              zlog_debug("dbdesc received");
              break;

            case OSPF6_MESSAGE_TYPE_LSREQ:
              zlog_debug("lsreq received");
              break;

            case OSPF6_MESSAGE_TYPE_LSUPDATE:
              zlog_debug("lsupdate received");
              break;

            case OSPF6_MESSAGE_TYPE_LSACK:
              zlog_debug("lsack received");
              break;

            default:
              zlog_debug("unknown message received, value: %d\n", oh->type);
              break;
          }

          append_msg_to_rcvd_queue(sr, msg);
          // first we check if all siblings are connected,
          // then we check if majority have received the messages
          if((n_ospf6_siblings == OSPF6_NUM_SIBS) &&
             (majority_have_msg_rcvd()))
          {
            // increment the current xid inside vote_majority
            vote_majority();
          }
        }
        else if(xid > sr->current_egress_xid)
        {
          // we need to append the message at the end of the list
          if(CONTROLLER_DEBUG_MSG)
          {
            zlog_debug("forward ospf6 packet: queued for later, xid %d from %s, current egress xid: %d", xid, sr->rconn->target, sr->current_egress_xid);
          }
          append_msg_to_rcvd_queue(sr, msg);
         }
        else
        {
          if(CONTROLLER_DEBUG_MSG)
          {
            zlog_debug("forward ospf6 packet: rejected, xid %d from %s, current egress xid: %d", xid, sr->rconn->target, sr->current_egress_xid);
          }
        }
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

    case RFPT_ACK:
      if(CONTROLLER_DEBUG_MSG)
      {
        zlog_debug("ACK for a previous RFPT message received, xid: %d", xid);
      }
      if(xid == sr->current_ingress_xid)
      {
        sr->current_ingress_ack_rcvd = true;

        if(CONTROLLER_DEBUG_MSG)
        {
          zlog_debug("xid in ACK corresponds to a previously sent message");
        }
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
sib_router_destroy(struct sib_router * sr)
{
  state_transition(sr, SIB_DISCONNECTED);

  // free the list
  while(!list_empty(&sr->msgs_rcvd_queue))
  {
    struct rfpbuf * msg = rfpbuf_from_list(list_pop_front(&sr->msgs_rcvd_queue));
    rfpbuf_delete(msg);
  }

  if(sr)
  {
    rconn_destroy(sr->rconn);

    if(sr->name)
    {
      free(sr->name);
    }

    if(sr->thread_timeout_ingress)
    {
      THREAD_OFF(sr->thread_timeout_ingress);
    }
  } 
}

static void
sib_router_send_features_reply(struct sib_router * sr, unsigned int xid)
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

    buffer = routeflow_alloc_xid(RFPT_FEATURES_REPLY, RFP10_VERSION, xid, sizeof(struct rfp_router_features));
  
    rfr = buffer->l2;

    for(i = 0; i < *n_routers_p; i++) // dereference the pointer
    {
      struct iflist_ * ifnode_;
      LIST_FOR_EACH(ifnode_, struct iflist_, node, &routers[i]->port_list)
      {
        struct interface * ifp = ifnode_->ifp;
        struct rfp_phy_port * rpp = rfpbuf_put_uninit(buffer, sizeof(struct rfp_phy_port));
        memset(rpp, 0, sizeof *rpp);
        rpp->port_no = htons(ifp->ifindex);
        rpp->state = htonl(ifp->state);
        rpp->mtu = htonl(ifp->mtu);
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
sib_router_send_address_reply(enum rfp_type type, struct sib_router * sr, struct connected * ifc, unsigned int ifindex, unsigned int xid)
{
  struct rfpbuf * buffer;
  int retval;

  if(type == RFPT_IPV4_ADDRESS_ADD)
  {
    struct rfp_ipv4_address * addr;

    buffer = routeflow_alloc_xid(type, RFP10_VERSION, xid, sizeof(struct rfp_ipv4_address));

    addr = buffer->l2;

    addr->ifindex = ifindex;
    addr->prefixlen = ifc->address->prefixlen;
    memcpy(&addr->p, &ifc->address->u.prefix, 4);
  }
  else if(type == RFPT_IPV6_ADDRESS_ADD)
  {
    struct rfp_ipv6_address * addr;

    buffer = routeflow_alloc_xid(type, RFP10_VERSION, xid, sizeof(struct rfp_ipv6_address));

    addr = buffer->l2;

    addr->ifindex = ifindex;
    addr->prefixlen = ifc->address->prefixlen;
    memcpy(&addr->p, &ifc->address->u.prefix, 16);
  }

  rfpmsg_update_length(buffer);
  retval = rconn_send(sr->rconn, buffer);

   if(retval)
  {
    printf("send to %s failed: %s",
           rconn_get_target(sr->rconn), strerror(retval));
  }
}

static void
sib_router_if_addr_add(struct sib_router * sr, unsigned int xid)
{
  int i;
  for(i = 0; i < *n_routers_p; i++)
  {
    struct iflist_ * ifnode_;
    LIST_FOR_EACH(ifnode_, struct iflist_, node, &routers[i]->port_list)
    {
      struct interface * ifp = ifnode_->ifp;
      struct connected * ifc;
      LIST_FOR_EACH(ifc, struct connected, node, &ifp->connected)
      {
        if(ifc->address->family == AF_INET)
        {
          sib_router_send_address_reply(RFPT_IPV4_ADDRESS_ADD, sr, ifc, ifp->ifindex, xid);
        }
        else if(ifc->address->family == AF_INET6)
        {
          sib_router_send_address_reply(RFPT_IPV6_ADDRESS_ADD, sr, ifc, ifp->ifindex, xid);
        }
      }
    }
  }
}

static void
sib_router_send_route_reply(enum rfp_type type, struct sib_router * sr, struct prefix * p, 
                            unsigned int xid, struct rib * rib)
{
  struct rfpbuf * buffer;
  struct rfp_ipv4_route * rir;
  int retval;

  buffer = routeflow_alloc_xid(type, RFP10_VERSION, xid, sizeof(struct rfp_ipv4_route));

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
sib_router_redistribute(struct sib_router * sr, unsigned int xid)
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
          sib_router_send_route_reply (RFPT_IPV4_ROUTE_ADD, sr, &rn->p, xid, newrib);
}

int sib_router_timeout_forward_ospf6(struct thread * thread)
{
  struct sib_router * sr;

  sr = (struct sib_router *)THREAD_ARG(thread);

  if(!sr->current_ingress_ack_rcvd)
  {
    if(CONTROLLER_DEBUG_MSG)
    {
      zlog_debug("Ack %d not received in time", sr->current_ingress_xid);
    }

    // Parse components
    uint64_t prefix, sisis_version, process_type, process_version, sys_id, pid, ts; 
    if(get_sisis_addr_components(sr->name, &prefix, &sisis_version, &process_type, &process_version, &sys_id, &pid, &ts) == 0)
      {
        if(CONTROLLER_DEBUG_MSG)
        {        
          zlog_debug("For address %s, %d\t%d\t%d\t%d\t%d\n", sr->name, process_type, process_version, sys_id, pid, ts);
        }

        // kill the sibling
        kill(pid, SIGKILL);
      }                                                                             
  }
  else
  {
    if(CONTROLLER_DEBUG_MSG)
    {
      zlog_debug("Ack %d was received in time", sr->current_ingress_xid);
    }
  }

  return 0;
}

void sib_router_forward_ospf6(struct rfpbuf * msg, unsigned int current_ingress_xid)
{
  int i;
  int retval;

  for(i = 0; i < n_ospf6_siblings; i++)
  {
    if(ospf6_siblings[i]->state == SIB_ROUTING)
    {
      struct rfpbuf * msg_copy = rfpbuf_clone(msg);
      ospf6_siblings[i]->current_ingress_xid = current_ingress_xid;
      ospf6_siblings[i]->current_ingress_ack_rcvd = false;

      // set a timeout for receiving the ack
      // *NOTE* take this timeout out for now
//      ospf6_siblings[i]->thread_timeout_ingress = thread_add_timer(master,
//                                                                   sib_router_timeout_forward_ospf6,
//                                                                   ospf6_siblings[i], 
//                                                                   ospf6_siblings[i]->ingress_ack_timeout);
//
      retval = rconn_send(ospf6_siblings[i]->rconn, msg_copy);
      if(retval)
      {
        printf("send to %s failed: %s\n",
    		    rconn_get_target(ospf6_siblings[i]->rconn), strerror(retval));
      }
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

void sib_router_send_leader_elect()
{
  int i;
  int retval;

  for(i = 0; i < n_ospf6_siblings; i++)
  {
    struct rfpbuf * buffer = routeflow_alloc_xid(RFPT_LEADER_ELECT, RFP10_VERSION, 
                                   ospf6_siblings[i]->current_ingress_xid, sizeof(struct rfp_header));
    retval = rconn_send(ospf6_siblings[i]->rconn, buffer);
 
  }
}

static void state_transition(struct sib_router * sr, enum sib_router_state state)
{
  // transition from a *_msg_received to routing
  if((sr->state == SIB_FEATURES_REDIS_REQ_RCVD || sr->state == SIB_FEATURES_ADDR_REQ_RCVD) &&
      state == SIB_ROUTING)
  {
    n_ospf6_siblings_routing++;
  }

  // transition from routing to disconnected
  if(sr->state == SIB_ROUTING &&
     state == SIB_DISCONNECTED)
  {
    n_ospf6_siblings_routing--;
  }

  sr->state = state;

  if(n_ospf6_siblings_routing == OSPF6_NUM_SIBS)
  {
    sib_router_send_leader_elect();
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
