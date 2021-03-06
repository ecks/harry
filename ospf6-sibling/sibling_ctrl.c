#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <netinet/in.h>

#include <riack.h>

#include "util.h"
#include "debug.h"
#include "dblist.h"
#include "prefix.h"
#include "rfpbuf.h"
#include "sisis.h"
#include "routeflow-common.h"
#include "thread.h"
#include "prefix.h"
#include "if.h"
#include "ospf6_interface.h" 
#include "ospf6_area.h"
#include "ospf6_route.h"
#include "ospf6_top.h"
#include "ospf6_db.h"
#include "ospf6_replica.h"
#include "ospf6_restart.h"
#include "ospf6_message.h"
#include "ctrl_client.h"
#include "sibling_ctrl.h"

struct list ctrl_clients;
struct list restart_msg_queue;

enum sibling_ctrl_state
{
  SC_INIT = 0,                         /* Sibling Ctrl has been initialized */
  SC_LEAD_ELECT_START = 1,             /* Leader Election Algorithm has been triggered */
  SC_ALL_INT_UP = 2,                   /* All the External Interfaces are up */
  SC_LEAD_ELECT_COMPL = 3,             /* Leader Election has been completed, however all interfaces are not up yet */
  SC_ALL_INT_UP_LEAD_ELECT_START = 4,  /* All interfaces are up, leader election algorithm has been triggered */
  SC_READY_TO_SEND_LEAD_ELECT_START = 5,/* Leader re-election due to a failed sibling, just perform leader election */
  SC_READY_TO_SEND = 6                /* Start sending OSPF6 messages, beginning with Hello messages */
};

enum sibling_ctrl_state state;

struct ospf6 * ospf6;

extern struct thread_master * master;

bool scheduled_hellos = false;

void sibling_ctrl_set_address(struct ctrl_client * ctrl_client,
                              struct in6_addr * ctrl_addr,
                              struct in6_addr * sibling_addr);

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
    ifp = if_get_by_name(ctrl_client->if_list, rpp->name);

    // fill up the interface info
    ifp->ifindex = ifindex;

    // fill up the mtu
    ifp->mtu = mtu;
    // copy over the flags
    ifp->state = ntohl(rpp->state);
    ospf6_interface_if_add(ifp, ctrl_client);
  }

  return 0;
}

int if_address_add_v4(struct ctrl_client * ctrl_client, struct rfpbuf * buffer)
{
  const struct rfp_ipv4_address * address = buffer->data;
  struct interface * ifp = if_lookup_by_index(ctrl_client->if_list, address->ifindex);
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
  struct interface * ifp = if_lookup_by_index(ctrl_client->if_list, address->ifindex);
  struct connected * ifc;

  struct prefix p;

  memcpy(&p.u.prefix, &address->p, 16);
  p.prefixlen = address->prefixlen;
  p.family = AF_INET6;

  ifc = connected_add_by_prefix(ifp, &p, NULL);
  if(ifc == NULL)
    return 1;

  if(IS_OSPF6_SIBLING_DEBUG_MSG)
  {
    char prefix_str[INET6_ADDRSTRLEN+3]; // three extra chars for slash + two digits
    if(prefix2str(ifc->address, prefix_str, INET6_ADDRSTRLEN) != 1)
    {   
      zlog_debug("v6 addr: %s", prefix_str, ifc->address->prefixlen);
    }   
  }

//  check here for:
//    1. prefix matches
//    2. extract hostnum from host portion of address
//    3. if hostnum matches, this is the internal interface that should be recorded

  char inter_target_str[INET6_ADDRSTRLEN+3]; // three extra chars for slash + two digits
  struct prefix inter_target_p;

  sprintf(inter_target_str, "2001:db8:beef:10::%x/64", ctrl_client->hostnum);
  str2prefix(inter_target_str, &inter_target_p);
  
  // link up the internal connected address with a pointer at ctrl_client
  if(prefix_same(ifc->address, &inter_target_p))
  {
    ctrl_client->inter_con = ifc;
  }
  
  // since connected address is AF_INET6, needs to be updated
  ospf6_interface_connected_route_update(ifc->ifp);

  return 0;
}

void sibling_ctrl_route_set(struct ospf6_route * route)
{
  struct ctrl_client * ctrl_client;

  struct ctrl_client * hostnum_ctrl_client;

  LIST_FOR_EACH(ctrl_client, struct ctrl_client, node, &ctrl_clients)
  {
//    if hostnum matches, then send as is
//    otherwise look at the ctrl that matches, and set the next hop to internal interface

    if(route->hostnum == ctrl_client->hostnum)
    {
      ctrl_client_route_set(ctrl_client, route);  
    }
    else
    {
      LIST_FOR_EACH(hostnum_ctrl_client, struct ctrl_client, node, &ctrl_clients)
      {
        if(route->hostnum == hostnum_ctrl_client->hostnum)
        {
          struct ospf6_route * route_copy = ospf6_route_copy(route);

          memcpy(&route_copy->nexthop[0].address, &hostnum_ctrl_client->inter_con->address->u.prefix, sizeof(struct in6_addr)); // copy the nexthop info first, this is the nexthop of the original route
          route_copy->nexthop[0].ifindex = ctrl_client->inter_con->ifp->ifindex;                     // ifindex to get there
          ctrl_client_route_set(ctrl_client, route_copy);  

          ospf6_route_delete(route_copy);
        }
//      route->prefix.prefixlen = ctrl_client->inter_con->address->prefixlen;
//      memcpy(&route->prefix.u.prefix, &ctrl_client->inter_con->address->u.prefix, sizeof(struct in6_addr));
      }
    }
  }
}

void sibling_ctrl_route_unset(struct ospf6_route * route)
{
  struct ctrl_client * ctrl_client;

  LIST_FOR_EACH(ctrl_client, struct ctrl_client, node, &ctrl_clients)
  {
    ctrl_client_route_unset(ctrl_client, route);
  }
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
  if(IS_OSPF6_SIBLING_DEBUG_MSG)
  {
    char prefix_str[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &(route->p->prefix.s_addr), prefix_str, INET_ADDRSTRLEN) != 1)
      zlog_debug("%s/%d", prefix_str, route->p->prefixlen);
  }
  return 0;
}

void sibling_ctrl_init() 
{
  list_init(&ctrl_clients);
  list_init(&restart_msg_queue);
  state = SC_INIT;
}

// global function
bool check_all_interface_up()
{
  bool all_interface_up = true;
  struct ctrl_client * ctrl_client;

  LIST_FOR_EACH(ctrl_client, struct ctrl_client, node, &ctrl_clients)
  {
    if( !(ctrl_client->state == CTRL_INTERFACE_UP || ctrl_client->state == CTRL_CONNECTED))
    {
      all_interface_up = false;
      break;
    }
  }

  return all_interface_up;
}

// helper function
bool check_all_rcvd_lead_elect()
{
  bool all_rcvd_lead_elect = true;
  struct ctrl_client * ctrl_client;

  LIST_FOR_EACH(ctrl_client, struct ctrl_client, node, &ctrl_clients)
  {
    if( !(ctrl_client->state == CTRL_LEAD_ELECT_RCVD || ctrl_client->state == CTRL_CONNECTED))
    {
      all_rcvd_lead_elect = false;
      break;
    }
  }

  return all_rcvd_lead_elect;
}

// helper function
void schedule_hellos_on_interfaces()
{
  struct interface * ifp;
  struct ospf6_interface * oi;
  struct ctrl_client * ctrl_client;

  // if we already scheduled hellos, dont need to do it again
  if(scheduled_hellos)
  {
    return;
  }

  LIST_FOR_EACH(ctrl_client, struct ctrl_client, node, &ctrl_clients)
  {
    ifp = if_get_by_name(ctrl_client->if_list, ctrl_client->interface_name);
    oi = (struct ospf6_interface *)ifp->info;

    thread_add_event(master, ospf6_hello_send, oi, 0);
  }

  scheduled_hellos = true;
}
 
// helper function - synchronize current xid with leader
void synchronize_current_xid()
{
  struct ctrl_client * ctrl_client;

  int egress_leader_xid = db_get_egress_xid(ospf6->riack_client);

  if(OSPF6_SIBLING_DEBUG_RESTART)
  {
    zlog_debug("synchronize_current_xid: Leader xid: %d", egress_leader_xid);
  }

  LIST_FOR_EACH(ctrl_client, struct ctrl_client, node, &ctrl_clients)
  {
    ctrl_client->current_xid = egress_leader_xid;
  }
}

bool past_leader_elect_start()
{
  if(state == SC_LEAD_ELECT_START || state == SC_ALL_INT_UP_LEAD_ELECT_START || state == SC_READY_TO_SEND_LEAD_ELECT_START)
    return true;
  return false; 
}

void sibling_ctrl_update_state(enum sc_state_update_req state_update_req)
{
  bool all_interface_up;
  bool all_rcvd_lead_elect;

  if(state_update_req == SCG_CTRL_INT_UP_REQ)
  {
    if(state == SC_INIT)
    {
      all_interface_up = check_all_interface_up();

      if(all_interface_up)
      {
        state = SC_ALL_INT_UP;
      }
    }
    else if(state == SC_LEAD_ELECT_START)
    {
      all_interface_up = check_all_interface_up();

      if(all_interface_up)
      {
        state = SC_ALL_INT_UP_LEAD_ELECT_START;
      }
    }
    else if(state == SC_LEAD_ELECT_COMPL)
    {
      all_interface_up = check_all_interface_up();

      if(all_interface_up)
      {
        state = SC_READY_TO_SEND;
        // schedule ospf6 hellos here
      }
 
    }
    else
    {
      if(IS_OSPF6_SIBLING_DEBUG_MSG)
      {
        zlog_debug("Sibling ctrl entered invalid state == current_state: %d, state_update_req: %d", state, state_update_req);
      }
    }
  }
  else if(state_update_req == SCG_CTRL_LEAD_ELECT_RCVD_REQ)
  {
    if(state == SC_INIT)
    {
      all_rcvd_lead_elect = check_all_rcvd_lead_elect();

      if(all_rcvd_lead_elect)
      {
        state = SC_LEAD_ELECT_START;
        ospf6_leader_elect();
      }
    }
    else if(state == SC_ALL_INT_UP)
    {
      all_rcvd_lead_elect = check_all_rcvd_lead_elect();

      if(all_rcvd_lead_elect)
      {
        state = SC_ALL_INT_UP_LEAD_ELECT_START;
        ospf6_leader_elect();
      }
    }
    else if(state == SC_READY_TO_SEND)
    {
      // leader election for a restarted sibling
      // redo leader election, however do not need to reschedule sending of hellos
      all_interface_up = check_all_interface_up();

      if(all_interface_up)
      {
        state = SC_READY_TO_SEND_LEAD_ELECT_START;
        ospf6_leader_elect();
      }
    }
    else
    {
      if(IS_OSPF6_SIBLING_DEBUG_MSG)
      {
        zlog_debug("Sibling ctrl entered invalid state == current_state: %d, state_update_req: %d", state, state_update_req);
      }
    }
 
  }
  else if(state_update_req == SCG_LEAD_ELECT_COMPL)
  {
    if(state == SC_LEAD_ELECT_START)
    {
      state = SC_LEAD_ELECT_COMPL;

      if(ospf6->restart_mode)
      {
        // only needed when not attached to external ospf6 
        pthread_mutex_unlock(&first_xid_mutex);

        // were going to synchronize xid and start scheduling hellos when we complete sibling restart 
        ospf6->restarted_first_egress_not_sent = true; // added temporarily
        schedule_hellos_on_interfaces(); // added temporarily
      }
      else
      {
        /* Schedule Hello for all interfaces */
        schedule_hellos_on_interfaces();
      }
    }
    else if(state == SC_ALL_INT_UP_LEAD_ELECT_START)
    {
      state = SC_READY_TO_SEND;

      if(ospf6->restart_mode)
      {
        // only needed when not attached to external ospf6 
        pthread_mutex_unlock(&first_xid_mutex);

        // were going to synchronize xid and start scheduling hellos when we complete sibling restart
        ospf6->restarted_first_egress_not_sent = true; // added temporarily
        schedule_hellos_on_interfaces(); // added temporarily
      }
      else
      {
        /* Schedule Hello */
        schedule_hellos_on_interfaces();
      }
    }
    else if(state == SC_READY_TO_SEND_LEAD_ELECT_START)
    {
      // sinche hellos are already scheduled, dont need to reschedule them
      state = SC_READY_TO_SEND;
    }
    else if(state == SC_LEAD_ELECT_COMPL || state == SC_READY_TO_SEND)
    {
      if(IS_OSPF6_SIBLING_DEBUG_MSG)
      {
        zlog_debug("Duplicate Leader Election Complete State");
      }
    }
    else
    {
      if(IS_OSPF6_SIBLING_DEBUG_MSG)
      {
        zlog_debug("Sibling ctrl entered invalid state == current_state: %d, state_update_req: %d", state, state_update_req);
      }
    }
  }
}

timestamp_t sibling_ctrl_ingress_timestamp()
{
  timestamp_t timestamp;
  long int timestamp_sec;
  long int timestamp_msec;

  zebralite_gettime(ZEBRALITE_CLK_REALTIME, &timestamp);
  timestamp_sec = (long int)timestamp.tv_sec;
  timestamp_msec = (long int)timestamp.tv_usec;

  if(IS_OSPF6_SIBLING_DEBUG_MSG)
  {
    zlog_debug("Populating timestamp: %ld,%ld", timestamp_sec, timestamp_msec);
  }

  return timestamp;
}

void sibling_ctrl_add_ctrl_client(unsigned int hostnum, char * ifname, char * area)
{
  struct ctrl_client * ctrl_client;
  struct interface * ifp;
  struct ospf6_interface * oi;
  struct ospf6_area * oa;

  u_int32_t area_id;

  ctrl_client = ctrl_client_new();
  ctrl_client->hostnum = hostnum;
  ctrl_client->inter_con = NULL;

  list_push_back(&ctrl_clients, &ctrl_client->node);

  /* find/create ospf6 interface */
  ifp = if_get_by_name(ctrl_client->if_list, ifname);
  oi = (struct ospf6_interface *) ifp->info;
  if(oi == NULL)
  {
    oi = ospf6_interface_create(ifp);
  }

  /* parse Area-ID */
  if (inet_pton (AF_INET, area, &area_id) != 1)  
  {
    printf("Invalid Area-ID: %s\n", area);
  }
  
  /* find/create ospf6 area */
  oa = ospf6_area_lookup (area_id, ospf6); 
  if(oa == NULL)
    oa = ospf6_area_create(area_id, ospf6); 
 
  /* attach interface to area */
  list_push_back(&oa->if_list, &oi->node);
  oi->area = oa;

  /* start up */
  thread_add_event(master, interface_up, oi, 0); 
}

void sibling_ctrl_set_addresses(struct in6_addr * sibling_ctrl)
{
  struct ctrl_client * ctrl_client;
  struct route_ipv6 * route_iter;

  LIST_FOR_EACH(ctrl_client, struct ctrl_client, node, &ctrl_clients)
  {
    struct list * ctrl_addrs = get_ctrl_addrs_for_hostnum(ctrl_client->hostnum);

    LIST_FOR_EACH(route_iter, struct route_ipv6, node, ctrl_addrs)
    {
      char s_addr[INET6_ADDRSTRLEN+1];
      inet_ntop(AF_INET6, &route_iter->p->prefix, s_addr, INET6_ADDRSTRLEN+1);
      zlog_debug("done getting ctrl addr %s for hostnum: %d ", s_addr, ctrl_client->hostnum);

      struct in6_addr * ctrl_addr = calloc(1, sizeof(struct in6_addr));
      memcpy(ctrl_addr, &route_iter->p->prefix, sizeof(struct in6_addr));
 
      sibling_ctrl_set_address(ctrl_client, ctrl_addr, sibling_ctrl);
    }

    // free  the list, no longer needed
    while(!list_empty(ctrl_addrs))
    {
      struct list * addr_to_remove = list_pop_front(ctrl_addrs);
      struct route_ipv6 * route_to_remove = CONTAINER_OF(addr_to_remove, struct route_ipv6, node);
      free(route_to_remove->p);
      free(route_to_remove->gate);
    }

    free(ctrl_addrs);
 
  }
}

void sibling_ctrl_set_address(struct ctrl_client * ctrl_client,
                              struct in6_addr * ctrl_addr,
                              struct in6_addr * sibling_addr)
{
  ctrl_client_init(ctrl_client, ctrl_addr, sibling_addr);

  ctrl_client->features_reply = recv_features_reply;
  ctrl_client->routes_reply = recv_routes_reply;
  ctrl_client->address_add_v4 = if_address_add_v4;
  ctrl_client->address_add_v6 = if_address_add_v6;
}

void sibling_ctrl_interface_init(struct ospf6_interface * oi)
{
  ctrl_client_interface_init(oi->ctrl_client, oi->interface->name);
}

struct interface * sibling_ctrl_if_lookup_by_name(const char * ifname)
{
  struct interface * ifp;
  struct ctrl_client * ctrl_client;

  LIST_FOR_EACH(ctrl_client, struct ctrl_client, node, &ctrl_clients)
  {
    ifp = if_lookup_by_name(ctrl_client->if_list, ifname);
    if(ifp != NULL &&
       ifp->info != NULL)  // make sure that ospf6_interface is initialized, otherwise we don't care about the interface
      return ifp;
  }

  return NULL;
}

struct interface * sibling_ctrl_if_lookup_by_index(int ifindex)
{
  struct interface * ifp;
  struct ctrl_client * ctrl_client;

  LIST_FOR_EACH(ctrl_client, struct ctrl_client, node, &ctrl_clients)
  {
    ifp = if_lookup_by_index(ctrl_client->if_list, ifindex);
    if(ifp != NULL &&
       ifp->info != NULL)
      return ifp;
  }

  return NULL;
}

//struct interface * sibling_ctrl_if_get_by_name(const char * ifname)
//{
//  struct ctrl_client * ctrl_client;

//  LIST_FOR_EACH(ctrl_client, struct ctrl_client, node, &ctrl_clients)
//  {
//    return if_get_by_name(ctrl_client->if_list, 
//  }
//  return NULL;
//}

// functions dealing with restart msg queue
int sibling_ctrl_first_xid_rcvd()
{
  struct list * first_msg_list;
  struct rfpbuf * first_msg_rcvd;
  struct rfp_header * rh;

  if((first_msg_list = list_peek_front(&restart_msg_queue)) == NULL)
    return -1;

  first_msg_rcvd = CONTAINER_OF(first_msg_list, struct rfpbuf, list_node);
  rh = rfpbuf_at_assert(first_msg_rcvd, 0, sizeof(struct rfp_header));
  return ntohl(rh->xid);
}

struct list * sibling_ctrl_restart_msg_queue()
{
  return &restart_msg_queue;
}

size_t sibling_ctrl_restart_msg_queue_num_msgs()
{
  return list_size(&restart_msg_queue);
}

int sibling_ctrl_push_to_restart_msg_queue(struct rfpbuf * msg_rcvd)
{
  list_push_back(&restart_msg_queue, &msg_rcvd->list_node);
}

void sibling_ctrl_redistribute_leader_elect(bool leader)
{
  struct ctrl_client * ctrl_client;

  LIST_FOR_EACH(ctrl_client, struct ctrl_client, node, &ctrl_clients)
  {
    ctrl_client_redistribute_leader_elect(ctrl_client, leader);
  }
}
