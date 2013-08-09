#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <netinet/in.h>

#include "thread.h"
#include "util.h"
#include "dblist.h"
#include "routeflow-common.h"
#include "if.h"
#include "ospf6_interface.h"
#include "ospf6_neighbor.h"

static void ospf6_neighbor_state_change(u_char next_state, struct ospf6_neighbor * on)
{
  u_char prev_state;

  prev_state = on->state;
  on->state = next_state;

  if(prev_state == next_state)
    return;

  // More to follow
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

//  quagga_gettime (QUAGGA_CLK_MONOTONIC, &on->last_changed);
  on->router_id = router_id;

//  on->summary_list = ospf6_lsdb_create (on);
//  on->request_list = ospf6_lsdb_create (on);
//  on->retrans_list = ospf6_lsdb_create (on);

//  on->dbdesc_list = ospf6_lsdb_create (on);
//  on->lsreq_list = ospf6_lsdb_create (on);
//  on->lsupdate_list = ospf6_lsdb_create (on);
//  on->lsack_list = ospf6_lsdb_create (on);

   list_push_back(&oi->neighbor_list, &on->node);

  return on;
}
