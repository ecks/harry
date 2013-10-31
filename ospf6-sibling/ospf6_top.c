#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h>

#include "routeflow-common.h"
#include "util.h"
#include "dblist.h"
#include "thread.h"
#include "vector.h"
#include "vty.h"
#include "command.h"
#include "if.h"
#include "ospf6_lsa.h"
#include "ospf6_lsdb.h"
#include "ospf6_area.h"
#include "ospf6_interface.h"
#include "ospf6_top.h"

/* global ospf6d variable */
struct ospf6 *ospf6;

extern struct thread_master * master;

static void ospf6_top_lsdb_hook_add(struct ospf6_lsa * lsa)
{
  // TODO
}

static void ospf6_top_lsdb_hook_remove(struct ospf6_lsa * lsa)
{
  // TODO
}

static struct ospf6 * ospf6_create(void)
{
  struct ospf6 * o;

  o = calloc(1, sizeof(struct ospf6));

  o->lsdb = ospf6_lsdb_create(o);
  o->lsdb->hook_add = ospf6_top_lsdb_hook_add;
  o->lsdb->hook_remove = ospf6_top_lsdb_hook_remove;
 
 list_init(&o->area_list);

  return o;
}

/* start ospf6 */
DEFUN(router_ospf6,
      router_ospf6_cmd,
      "router ospf6",
      ROUTER_STR
      OSPF6_STR)
{
  if(ospf6 == NULL)
    ospf6 = ospf6_create();

  /* set current ospf point */
  vty->node = OSPF6_NODE;
  vty->index = ospf6;

  return CMD_SUCCESS;
}

/* change Router_ID commands */
DEFUN(ospf6_router_id,
      ospf6_router_id_cmd,
      "router-id A.B.C.D",
      "Cnofigure OSPF Router-ID\n"
      V4NOTATION_STR)
{
  int ret;
  u_int32_t router_id;
  struct ospf6 * o;

  o = (struct ospf6 *)vty->index;

  ret = inet_pton(AF_INET, argv[0], &router_id);
  if(ret == 0)
  {
    // error
    return CMD_SUCCESS;
  }

  if(o->router_id == 0)
    o->router_id = router_id;

  return CMD_SUCCESS;
}

DEFUN(ospf6_interface_area,
      ospf6_interface_area_cmd,
      "interface IFNAME area A.B.C.D",
      "Enable routing on an IPv6 interface\n"
      IFNAME_STR
      "Specify the OSPF6 area ID\n"
      "OSPF6 area ID in IPv4 address notation\n")
{
  struct ospf6 *o;
  struct ospf6_area *oa;
  struct interface * ifp;
  struct ospf6_interface * oi;
  u_int32_t area_id;

  o = (struct ospf6 *)vty->index;

  /* find/create ospf6 interface */
  ifp = if_get_by_name(argv[0]);
  oi = (struct ospf6_interface *) ifp->info;
  if(oi == NULL)
  {
    oi = ospf6_interface_create(ifp);
  }

  /* parse Area-ID */
  if (inet_pton (AF_INET, argv[1], &area_id) != 1) 
  {
    printf("Invalid Area-ID: %s\n", argv[1]);
  }

  /* find/create ospf6 area */
  oa = ospf6_area_lookup (area_id, o);
  if(oa == NULL)
    oa = ospf6_area_create(area_id, o);

  oi->area = oa;

  /* start up */
  thread_add_event(master, interface_up, oi, 0);

  return CMD_SUCCESS;
}

DEFUN(no_ospf6_interface_area,
      no_ospf6_interface_area_cmd,
      "no interface IFNAME area A.B.C.D",
      NO_STR
      "Disable routing on an IPv6 interface\n"
      IFNAME_STR
      "Specify the OSPF6 area ID\n"
      "OSPF6 area ID in IPv4 address notation\n")
{
  return CMD_SUCCESS;
}

/* OSPF configuration write function. */
static int
config_write_ospf6(struct vty * vty)
{
  // TODO
  
  return 0;
}

/* OSPF6 node structure */
static struct cmd_node ospf6_node =
{
  OSPF6_NODE,
  "%s(config-ospf6)# ",
  1 /* VTYSH */
};

void ospf6_top_init()
{
  /* Install ospf6 top node */
  install_node(&ospf6_node, config_write_ospf6);

  install_element(CONFIG_NODE, &router_ospf6_cmd);
 
  install_default(OSPF6_NODE);
  install_element(OSPF6_NODE, &ospf6_router_id_cmd);
  install_element(OSPF6_NODE, &ospf6_interface_area_cmd);
  install_element(OSPF6_NODE, &no_ospf6_interface_area_cmd);
}
