#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>
#include <netinet/in.h>

#include "thread.h"
#include "util.h"
#include "dblist.h"
#include "routeflow-common.h"
#include "debug.h"
#include "if.h"
#include "ospf6_top.h"
#include "ospf6_lsa.h"
#include "ospf6_flood.h"
#include "ospf6_lsdb.h"
#include "ospf6_proto.h"
#include "ospf6_interface.h"
#include "ospf6_message.h"
#include "ospf6_neighbor.h"

/* global ospf6d variable */
struct ospf6 *ospf6;

const char *ospf6_neighbor_state_str[] =
{ "None", "Down", "Attempt", "Init", "Twoway", "ExStart", "ExChange",
    "Loading", "Full", NULL };

extern struct thread_master * master;

static void ospf6_neighbor_state_change(u_char next_state, struct ospf6_neighbor * on)
{
  u_char prev_state;

  prev_state = on->state;
  on->state = next_state;

  if(prev_state == next_state)
    return;

  if(IS_OSPF6_SIBLING_DEBUG_NEIGHBOR)
  {
    zlog_debug("Neighbor state change %s: [%s]->[%s]", on->name, ospf6_neighbor_state_str[prev_state], ospf6_neighbor_state_str[next_state]);
  }
}

static int need_adjacency(struct ospf6_neighbor * on)
{
  if(on->ospf6_if->state == OSPF6_INTERFACE_POINTTOPOINT ||
     on->ospf6_if->state == OSPF6_INTERFACE_DR ||
     on->ospf6_if->state == OSPF6_INTERFACE_BDR)
    return 1;

  if (on->ospf6_if->drouter == on->router_id ||
    on->ospf6_if->bdrouter == on->router_id)
    return 1;

  if(IS_OSPF6_SIBLING_DEBUG_NEIGHBOR)
  {
    zlog_debug("adjacency not needed, if router id: %d, on router id: %d", on->ospf6_if->drouter, on->router_id);
  }

  return 0;
}

int hello_received(struct thread * thread)
{
  struct ospf6_neighbor * on;

  on = THREAD_ARG(thread);
  assert(on);

  printf("Hello Received\n");

  // TODO: reset Inactivity Timer
  
  if(on->state <= OSPF6_NEIGHBOR_DOWN)
      ospf6_neighbor_state_change(OSPF6_NEIGHBOR_INIT, on);

  return 0;
}      

int twoway_received(struct thread * thread)
{
  struct ospf6_neighbor * on;

  on = (struct ospf6_neighbor *)THREAD_ARG(thread);
  assert(on);

  if(on->state > OSPF6_NEIGHBOR_INIT)
    return 0;

  if(IS_OSPF6_SIBLING_DEBUG_NEIGHBOR)
  {
    zlog_debug("Neighbor Event %s: *2Way-Received*", on->name);
  }

  thread_add_event(master, neighbor_change, on->ospf6_if, 0);

  if(! need_adjacency(on))
  {
    if(IS_OSPF6_SIBLING_DEBUG_NEIGHBOR)
    {
      zlog_debug("Changing neighbor state to twoway, adjacency not needed");
    }

    ospf6_neighbor_state_change(OSPF6_NEIGHBOR_TWOWAY, on);
    return 0;
  }

  ospf6_neighbor_state_change(OSPF6_NEIGHBOR_EXSTART, on);
  SET_FLAG(on->dbdesc_bits, OSPF6_DBDESC_MSBIT);
  SET_FLAG(on->dbdesc_bits, OSPF6_DBDESC_MBIT);
  SET_FLAG(on->dbdesc_bits, OSPF6_DBDESC_IBIT);

  THREAD_OFF(on->thread_send_dbdesc);
  on->thread_send_dbdesc = thread_add_event(master, ospf6_dbdesc_send, on, 0);

  return 0;
}

int negotiation_done(struct thread * thread)
{
  struct ospf6_neighbor * on;
  struct ospf6_lsa * lsa;

  on = (struct ospf6_neighbor *) THREAD_ARG(thread);
  assert(on);

  if(on->state != OSPF6_NEIGHBOR_EXSTART)
    return 0;

  if(IS_OSPF6_SIBLING_DEBUG_NEIGHBOR)
    zlog_debug("Neighbor Event %s: *NegotiationDone*", on->name);

  /* clear ls-list */
  ospf6_lsdb_remove_all(on->summary_list);
  ospf6_lsdb_remove_all(on->request_list);

  for(lsa = ospf6_lsdb_head(on->retrans_list); lsa;
      lsa = ospf6_lsdb_next(lsa))
  {
    ospf6_decrement_retrans_count(lsa);
    ospf6_lsdb_remove(lsa, on->retrans_list);
  }

  /* Interface scoped LSAs */
  for(lsa = ospf6_lsdb_head(on->ospf6_if->lsdb); lsa;
      lsa = ospf6_lsdb_next(lsa))
  {
    if (OSPF6_LSA_IS_MAXAGE (lsa))
    {
      ospf6_increment_retrans_count(lsa);
      ospf6_lsdb_add(ospf6_lsa_copy(lsa), on->retrans_list);
    }
    else
      ospf6_lsdb_add(ospf6_lsa_copy(lsa), on->summary_list);
  }

  /* Area scoped LSAs - no area implemented yet */
//  LIST_FOR_EACH(lsa, struct ospf6_lsa, node, on-> )
//  {

//  }

  /* AS scoped LSAs */
  for(lsa = ospf6_lsdb_head(ospf6->lsdb); lsa;
      lsa = ospf6_lsdb_next(lsa))
  {
    if(OSPF6_LSA_IS_MAXAGE(lsa))
    {
      ospf6_increment_retrans_count(lsa);
      ospf6_lsdb_add(ospf6_lsa_copy(lsa), on->retrans_list);
    }
    else
      ospf6_lsdb_add(ospf6_lsa_copy(lsa), on->summary_list);
  }
 

  UNSET_FLAG(on->dbdesc_bits, OSPF6_DBDESC_IBIT);
  ospf6_neighbor_state_change(OSPF6_NEIGHBOR_EXCHANGE, on);

  return 0;
}

int adj_ok(struct thread * thread)
{
  struct ospf6_neighbor * on;

  on = (struct ospf6_neighbor *)THREAD_ARG(thread);
  assert(on);

  if(IS_OSPF6_SIBLING_DEBUG_NEIGHBOR)
  {
    zlog_debug("Neighbor Event %s: *AdjOK?*", on->name);
  }

  if(on->state == OSPF6_NEIGHBOR_TWOWAY && need_adjacency(on))
  {
    ospf6_neighbor_state_change(OSPF6_NEIGHBOR_EXSTART, on);

    // send dbdesc here
  }
  else if(on->state >= OSPF6_NEIGHBOR_EXSTART && !need_adjacency(on))
  {
    // remove all lsas
  }

  return 0;
}

int oneway_received(struct thread * thread)
{
  struct ospf6_neighbor * on;

  on = (struct ospf6_neighbor *)THREAD_ARG(thread);

  if(on->state < OSPF6_NEIGHBOR_TWOWAY)
    return 0;

  if(IS_OSPF6_SIBLING_DEBUG_NEIGHBOR)
    zlog_debug("Neighbor Event %s: *1Way-Received*", on->name);

  ospf6_neighbor_state_change(OSPF6_NEIGHBOR_INIT, on);
  thread_add_event(master, neighbor_change, on->ospf6_if, 0);

  return 0;
}

struct ospf6_neighbor * ospf6_neighbor_lookup(u_int32_t router_id, struct ospf6_interface *oi)
{
  struct ospf6_neighbor * on; 
  LIST_FOR_EACH(on, struct ospf6_neighbor, node, &oi->neighbor_list)
    if(on->router_id == router_id)
      return on;

  return (struct ospf6_neighbor *) NULL;
}

/* create ospf6 neighbor */
struct ospf6_neighbor * ospf6_neighbor_create(u_int32_t router_id, struct ospf6_interface * oi)
{
  struct ospf6_neighbor * on;
  char buf[16];

  on = (struct ospf6_neighbor *)calloc(1, sizeof(struct ospf6_neighbor));
  if(on == NULL)
  {
    printf("malloc failed\n");
    return NULL;
  }

  memset (on, 0, sizeof (struct ospf6_neighbor));
  inet_ntop (AF_INET, &router_id, buf, sizeof (buf));
  snprintf (on->name, sizeof (on->name), "%s%%%s",
            buf, oi->interface->name);
  on->ospf6_if = oi; 
  on->state = OSPF6_NEIGHBOR_DOWN; 

  zebralite_gettime(ZEBRALITE_CLK_MONOTONIC, &on->last_changed);
  on->router_id = router_id;

  on->summary_list = ospf6_lsdb_create (on);
  on->request_list = ospf6_lsdb_create (on);
  on->retrans_list = ospf6_lsdb_create (on);

  on->dbdesc_list = ospf6_lsdb_create (on);
  on->lsreq_list = ospf6_lsdb_create (on);
  on->lsupdate_list = ospf6_lsdb_create (on);
  on->lsack_list = ospf6_lsdb_create (on);

   list_push_back(&oi->neighbor_list, &on->node);

  return on;
}
