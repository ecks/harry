#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>

#include "routeflow-common.h"
#include "util.h"
#include "dblist.h"
#include "debug.h"
#include "thread.h"
#include "if.h"
#include "ospf6_top.h"
#include "ospf6_lsa.h"
#include "ospf6_lsdb.h"
#include "ospf6_interface.h"
#include "ospf6_neighbor.h"
#include "ospf6_message.h"

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

static void ospf6_interface_lsdb_hook(struct ospf6_lsa * lsa)
{
  // TODO
}

/* Create new ospf6 interface structure */
struct ospf6_interface * ospf6_interface_create (struct interface *ifp)
{
    struct ospf6_interface *oi;

    oi = calloc(1, sizeof(struct ospf6_interface));

    oi->area = NULL;
    oi->priority = 1;

    oi->hello_interval = 10;
    oi->dead_interval = 40;
    oi->rxmt_interval = 5;
 
    oi->state = OSPF6_INTERFACE_DOWN;
 
    list_init(&oi->neighbor_list);

    oi->lsdb = ospf6_lsdb_create(oi);
    oi->lsdb->hook_add = ospf6_interface_lsdb_hook;
    oi->lsdb->hook_remove = ospf6_interface_lsdb_hook;
    oi->interface = ifp;
    ifp->info = oi;

    return oi;
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
  // not sure what this is for yet
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
  sibling_ctrl_interface_init(oi->interface->name);

  /* Update intreface route */
  ospf6_interface_connected_route_update(oi->interface);

  /* Schedule Hello */
  thread_add_event(master, ospf6_hello_send, oi, 0);

  /* decide next interface state */
  if(if_is_pointtopoint(oi->interface))
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
  struct ospf6_intreface * oi;

  oi = THREAD_ARG(thread);
//  assert(oi && oi->interface);

  // dr election


  return 0;
}
