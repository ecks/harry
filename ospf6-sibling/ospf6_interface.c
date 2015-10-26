#include "config.h"

#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "routeflow-common.h"
#include "util.h"
#include "dblist.h"
#include "debug.h"
#include "thread.h"
#include "if.h"
#include "prefix.h"
#include "riack.h"
#include "ospf6_top.h"
#include "ospf6_lsa.h"
#include "ospf6_lsdb.h"
#include "ospf6_route.h"
#include "ospf6_area.h"
#include "ospf6_intra.h"
#include "ospf6_interface.h"
#include "ospf6_restart.h"
#include "ospf6_neighbor.h"
#include "ospf6_message.h"
#include "ctrl_client.h"
#include "sibling_ctrl.h"

/* global ospf6d variable */
struct ospf6 *ospf6;

extern struct thread_master * master;

const char * ospf6_interface_state_str[] =
{
  "None",
  "Down",
  "Loopback",
  "Waiting",
  "PointToPoint",
  "DRother",
  "BDR",
  "DR",
  NULL
};

struct ospf6_interface * ospf6_interface_lookup_by_ifindex (int ifindex)
{
  struct ospf6_interface *oi; 
  struct interface *ifp;

  ifp = sibling_ctrl_if_lookup_by_index (ifindex);
  if (ifp == NULL)
    return (struct ospf6_interface *) NULL;

  oi = (struct ospf6_interface *) ifp->info;
  return oi;
}

static void ospf6_interface_lsdb_hook(struct ospf6_lsa * lsa)

{
  switch (ntohs (lsa->header->type))
  {    
    case OSPF6_LSTYPE_LINK:
      if (OSPF6_INTERFACE (lsa->lsdb->data)->state == OSPF6_INTERFACE_DR)
        OSPF6_INTRA_PREFIX_LSA_SCHEDULE_TRANSIT (OSPF6_INTERFACE (lsa->lsdb->data));
      ospf6_spf_schedule (OSPF6_INTERFACE (lsa->lsdb->data)->area, OSPF6_INTERFACE (lsa->lsdb->data)->ctrl_client->hostnum);
      break;

    default:
      break;
  }    
}

/* Create new ospf6 interface structure */
struct ospf6_interface * ospf6_interface_create (struct interface *ifp)
{
  struct ospf6_interface *oi;

  oi = calloc(1, sizeof(struct ospf6_interface));

  oi->area = NULL;
  oi->transdelay = 1;
  oi->priority = 1;

  oi->hello_interval = 10;
  oi->dead_interval = 600;
  oi->rxmt_interval = 5;
  oi->cost = 1;
  oi->state = OSPF6_INTERFACE_DOWN;
 
  list_init(&oi->neighbor_list);

  oi->lsupdate_list = ospf6_lsdb_create(oi);
  oi->lsack_list = ospf6_lsdb_create(oi);
  oi->lsdb = ospf6_lsdb_create(oi);
  oi->lsdb->hook_add = ospf6_interface_lsdb_hook;
  oi->lsdb->hook_remove = ospf6_interface_lsdb_hook;
  oi->lsdb_self = ospf6_lsdb_create(oi);

  oi->route_connected = OSPF6_ROUTE_TABLE_CREATE(INTERFACE, CONNECTED_ROUTES);
  oi->route_connected->scope = oi;

  oi->interface = ifp;
  ifp->info = oi;

  // node to be added to area list
  list_init(&oi->node);

  return oi;
}

static struct in6_addr *
ospf6_interface_get_linklocal_address(struct interface * ifp)
{
  struct connected * c;
  struct in6_addr * l = (struct in6_addr *) NULL;
 
  LIST_FOR_EACH(c, struct connected, node, &ifp->connected)
  {
    /* if family not AF_INET6, ignore */
    if(c->address->family != AF_INET6)
      continue;

    /* linklocal scope check */
    if(IN6_IS_ADDR_LINKLOCAL(&c->address->u.prefix6))
        l = &c->address->u.prefix6;
  }

  return l;
}

void ospf6_interface_if_add(struct interface * ifp, struct ctrl_client * ctrl_client)
{
 struct ospf6_interface * oi = (struct ospf6_interface *)ifp->info;
  if(oi == NULL)
    return;

  oi->ctrl_client = ctrl_client;

  // set the mtu since we know it now
  oi->ifmtu = ifp->mtu;

  /* Interface start */
  thread_add_event(master, interface_up, oi, 0);
}


void ospf6_interface_connected_route_update(struct interface * ifp)
{
  struct ospf6_interface * oi;
  struct ospf6_route * route;
  struct connected * ifc;

  oi = (struct ospf6_interface *)ifp->info;
  if(oi == NULL)
    return;

  /* reset linklocal pointer */
  oi->linklocal_addr = ospf6_interface_get_linklocal_address(ifp);

  // if area is null (no area for now)

  /* update "route to advertise" interface route table */
  ospf6_route_remove_all(oi->route_connected);

  LIST_FOR_EACH(ifc, struct connected, node, &ifp->connected)
  {
    if(ifc->address->family != AF_INET6)
      continue;

   CONTINUE_IF_ADDRESS_LINKLOCAL (IS_OSPF6_SIBLING_DEBUG_INTERFACE, ifc->address);
   CONTINUE_IF_ADDRESS_UNSPECIFIED (IS_OSPF6_SIBLING_DEBUG_INTERFACE, ifc->address);
   CONTINUE_IF_ADDRESS_LOOPBACK (IS_OSPF6_SIBLING_DEBUG_INTERFACE, ifc->address);
   CONTINUE_IF_ADDRESS_V4COMPAT (IS_OSPF6_SIBLING_DEBUG_INTERFACE, ifc->address);
   CONTINUE_IF_ADDRESS_V4MAPPED (IS_OSPF6_SIBLING_DEBUG_INTERFACE, ifc->address);

   /* apply filter */

   route = ospf6_route_create();
   memcpy(&route->prefix, ifc->address, sizeof(struct prefix));
   apply_mask(&route->prefix);
   route->type = OSPF6_DEST_TYPE_NETWORK;
   // TODO: path
//   route->path.area_id = oi->area->area_id;
//   route->path.type = OSPF6_PATH_TYPE_INTRA;
//   route->path.cost = oi->cost;
   route->nexthop[0].ifindex = oi->interface->ifindex;
   inet_pton(AF_INET6, "::1", &route->nexthop[0].address);
   ospf6_route_add(route, oi->route_connected);
  }

  /* create new Link-LSA */
  OSPF6_LINK_LSA_SCHEDULE(oi);
//  OSPF6_INTRA_PREFIX_LSA_SCHEDULE_TRANSIT(oi);
//  OSPF6_INTRA_PREFIX_LSA_SCHEDULE_STUB(oi->area);
}

static void ospf6_interface_state_change(u_char next_state, struct ospf6_interface * oi)
{
  u_char prev_state;

  prev_state = oi->state;

  if(prev_state == next_state)
    return;

  if(IS_OSPF6_SIBLING_DEBUG_INTERFACE)
  {
    zlog_debug("Interface state change %s: %s -> %s", oi->interface->name, ospf6_interface_state_str[prev_state], ospf6_interface_state_str[next_state]);
  }

  oi->state = next_state;
}

#define IS_ELIGIBLE(n) \
    ((n)->state >= OSPF6_NEIGHBOR_TWOWAY && (n)->priority != 0)

static struct ospf6_neighbor * better_bdrouter(struct ospf6_neighbor * a, struct ospf6_neighbor * b)
{
  if ((a == NULL || ! IS_ELIGIBLE (a) || a->drouter == a->router_id) &&
      (b == NULL || ! IS_ELIGIBLE (b) || b->drouter == b->router_id))
    return NULL;
  else if (a == NULL || ! IS_ELIGIBLE (a) || a->drouter == a->router_id)
    return b;
  else if (b == NULL || ! IS_ELIGIBLE (b) || b->drouter == b->router_id)
    return a;

  if (a->bdrouter == a->router_id && b->bdrouter != b->router_id)
    return a;
  if (a->bdrouter != a->router_id && b->bdrouter == b->router_id)
    return b;

  if (a->priority > b->priority)
    return a;
  if (a->priority < b->priority)
    return b;

  if (ntohl (a->router_id) > ntohl (b->router_id))
    return a;
  if (ntohl (a->router_id) < ntohl (b->router_id))
    return b;

  zlog_debug ("Router-ID duplicate ?");
    return a;
}

static struct ospf6_neighbor * better_drouter(struct ospf6_neighbor * a, struct ospf6_neighbor * b)
{
  if ((a == NULL || ! IS_ELIGIBLE (a) || a->drouter != a->router_id) &&
      (b == NULL || ! IS_ELIGIBLE (b) || b->drouter != b->router_id))
    return NULL;
  else if (a == NULL || ! IS_ELIGIBLE (a) || a->drouter != a->router_id)
    return b;
  else if (b == NULL || ! IS_ELIGIBLE (b) || b->drouter != b->router_id)
    return a;

  if (a->drouter == a->router_id && b->drouter != b->router_id)
    return a;
  if (a->drouter != a->router_id && b->drouter == b->router_id)
    return b;

  if (a->priority > b->priority)
    return a;
  if (a->priority < b->priority)
    return b;

  if (ntohl (a->router_id) > ntohl (b->router_id))
    return a;
  if (ntohl (a->router_id) < ntohl (b->router_id))
    return b;

  zlog_debug ("Router-ID duplicate ?");
  return a;
}

static u_char dr_election(struct ospf6_interface * oi)
{
  struct ospf6_neighbor * on, * drouter, * bdrouter, myself;
  struct ospf6_neighbor * best_drouter, * best_bdrouter;
  u_char next_state;

  drouter = bdrouter = NULL;
  best_drouter = best_bdrouter = NULL;

  drouter = bdrouter = NULL;
  best_drouter = best_bdrouter = NULL;

  /* pseudo neighbor myself, including noting current DR/BDR (1) */
  memset (&myself, 0, sizeof (myself));
  inet_ntop (AF_INET, &ospf6->router_id, myself.name,      // should really be oi->area->ospf6->router_id
             sizeof (myself.name));
  myself.state = OSPF6_NEIGHBOR_TWOWAY;
  myself.drouter = oi->drouter;
  myself.bdrouter = oi->bdrouter;
  myself.priority = oi->priority;
  myself.router_id = ospf6->router_id;          // should really be oi->area->ospf6->router_id

  /* Electing BDR (2) */
  LIST_FOR_EACH(on, struct ospf6_neighbor, node, &oi->neighbor_list)
    bdrouter = better_bdrouter (bdrouter, on); 
                    
  best_bdrouter = bdrouter;
  bdrouter = better_bdrouter (best_bdrouter, &myself);

  /* Electing DR (3) */
  LIST_FOR_EACH(on, struct ospf6_neighbor, node, &oi->neighbor_list) 
    drouter = better_drouter (drouter, on); 

  best_drouter = drouter;
  drouter = better_drouter (best_drouter, &myself);
  if (drouter == NULL)
    drouter = bdrouter;

  /* the router itself is newly/no longer DR/BDR (4) */
  if ((drouter == &myself && myself.drouter != myself.router_id) ||
    (drouter != &myself && myself.drouter == myself.router_id) ||
    (bdrouter == &myself && myself.bdrouter != myself.router_id) ||
    (bdrouter != &myself && myself.bdrouter == myself.router_id))
  {
    myself.drouter = (drouter ? drouter->router_id : htonl (0));
    myself.bdrouter = (bdrouter ? bdrouter->router_id : htonl (0));

    /* compatible to Electing BDR (2) */
    bdrouter = better_bdrouter (best_bdrouter, &myself);

    /* compatible to Electing DR (3) */
    drouter = better_drouter (best_drouter, &myself);
    if (drouter == NULL)
      drouter = bdrouter;
  }

  /* Set interface state accordingly (5) */
  if (drouter && drouter == &myself)
    next_state = OSPF6_INTERFACE_DR;
  else if (bdrouter && bdrouter == &myself)
    next_state = OSPF6_INTERFACE_BDR;
  else
    next_state = OSPF6_INTERFACE_DROTHER;

  if(IS_OSPF6_SIBLING_DEBUG_INTERFACE)
  {
    zlog_debug("DR Election");
  }

  /* If DR or BDR change, invoke AdjOK? for each neighbor (7) */
  /* RFC 2328 section 12.4. Originating LSAs (3) will be handled
     accordingly after AdjOK */
  if (oi->drouter != (drouter ? drouter->router_id : htonl (0)) ||
      oi->bdrouter != (bdrouter ? bdrouter->router_id : htonl (0)))
  {
    if (IS_OSPF6_SIBLING_DEBUG_INTERFACE)
      zlog_debug ("DR Election on %s: DR: %s BDR: %s", oi->interface->name,
                  (drouter ? drouter->name : "0.0.0.0"),
                  (bdrouter ? bdrouter->name : "0.0.0.0"));

    LIST_FOR_EACH(on, struct ospf6_neighbor, node, &oi->neighbor_list)
    {
      if (on->state < OSPF6_NEIGHBOR_TWOWAY)
        continue;
      /* Schedule AdjOK. */
      thread_add_event (master, adj_ok, on, 0);
    }
  }

  oi->drouter = (drouter ? drouter->router_id : htonl (0));
  oi->bdrouter = (bdrouter ? bdrouter->router_id : htonl (0));
  return next_state;
}

int interface_up(struct thread * thread)
{
  struct ospf6_interface * oi;

  oi = THREAD_ARG(thread);
  assert(oi && oi->interface);
  
  /* check that physical interface is up */
  if( !if_is_up(oi->interface))
  {
    return 0;
  }

  /* if already enabled, do nothing */
  if(oi->state > OSPF6_INTERFACE_DOWN)
  {
    return 0;
  }

  /* Update ctrl client with interface name */
  sibling_ctrl_interface_init(oi);

  /* Start state catchup if running in restart mode */
  if(ospf6->restart_mode)
  {
    ospf6_restart_init(oi, false);
  }

  /* Update interface route */
  ospf6_interface_connected_route_update(oi->interface);

  // mutex lock
//  if(!ospf6->restart_mode) // the following if statement is prob not necessary
//  {
    /* Schedule Hello */
    // thread_add_event(master, ospf6_hello_send, oi, 0);
    // Dont send hellos yet, just update ctrl_client that interface is up
 
    ctrl_client_state_transition(oi->ctrl_client, CTRL_INTERFACE_UP);
//  }
  // mutex unlock
 
  /* decide next interface state */
  if(if_is_pointopoint(oi->interface))
    ospf6_interface_state_change(OSPF6_INTERFACE_POINTTOPOINT, oi);
  else if(oi->priority == 0)
    ospf6_interface_state_change(OSPF6_INTERFACE_DROTHER, oi);
  else
  {
    ospf6_interface_state_change(OSPF6_INTERFACE_WAITING, oi);
    thread_add_timer(master, wait_timer, oi, oi->dead_interval);
  }

  return 0;
}

int wait_timer(struct thread * thread)
{
  struct ospf6_interface * oi;

  oi = (struct ospf6_interface *)THREAD_ARG(thread);
  assert(oi && oi->interface);

  if(IS_OSPF6_SIBLING_DEBUG_INTERFACE)
  {
    zlog_debug("Interface Event %s: [WaitTimer]", oi->interface->name);
  }

  if(oi->state == OSPF6_INTERFACE_WAITING)
    ospf6_interface_state_change(dr_election(oi), oi);
}

int neighbor_change(struct thread * thread)
{
  struct ospf6_interface * oi;

  oi = THREAD_ARG(thread);
  assert(oi && oi->interface);

  if (oi->state == OSPF6_INTERFACE_DROTHER ||
      oi->state == OSPF6_INTERFACE_BDR ||
      oi->state == OSPF6_INTERFACE_DR)
    ospf6_interface_state_change (dr_election (oi), oi); 


  return 0;
}

int interface_down (struct thread *thread)
{
  struct ospf6_interface *oi;
  struct ospf6_neighbor *on; 

  oi = (struct ospf6_interface *) THREAD_ARG (thread);
  assert (oi && oi->interface);

  if (IS_OSPF6_SIBLING_DEBUG_INTERFACE)
    zlog_debug ("Interface Event %s: [InterfaceDown]",
                oi->interface->name);

  /* TODO: Leave AllSPFRouters */
//  if (oi->state > OSPF6_INTERFACE_DOWN)
//    ospf6_leave_allspfrouters (oi->interface->ifindex);

  ospf6_interface_state_change (OSPF6_INTERFACE_DOWN, oi); 

  LIST_FOR_EACH(on, struct ospf6_neighbor, node, &oi->neighbor_list) 
    ospf6_neighbor_delete (on);
  
//  list_delete_all_node (oi->neighbor_list);

  return 0;
}
