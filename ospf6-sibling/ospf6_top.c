#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h>

#include "riack.h"

#include "routeflow-common.h"
#include "util.h"
#include "dblist.h"
#include "prefix.h"
#include "thread.h"
#include "vector.h"
#include "vty.h"
#include "command.h"
#include "if.h"
#include "db.h"
#include "ospf6_lsa.h"
#include "ospf6_lsdb.h"
#include "ospf6_area.h"
#include "ospf6_interface.h"
#include "ospf6_route.h"
#include "ospf6_top.h"

/* global ospf6d variable */
struct ospf6 *ospf6;

extern struct thread_master * master;

static bool my_restart_mode;

static void ospf6_top_lsdb_hook_add(struct ospf6_lsa * lsa)
{
  // TODO
}

static void ospf6_top_lsdb_hook_remove(struct ospf6_lsa * lsa)
{
  // TODO
}

static void ospf6_top_route_hook_add(struct ospf6_route * route)
{
  // ospf6_abr_originate_summary (route);
  sibling_ctrl_route_set(route);
}

static void ospf6_top_route_hook_remove(struct ospf6_route * route)
{
  // ospf6_abr_originate_summary (route);
  sibling_ctrl_route_unset(route);
}

static struct ospf6 * ospf6_create(void)
{
  struct ospf6 * o;

  o = calloc(1, sizeof(struct ospf6));

  o->lsdb = ospf6_lsdb_create(o);
  o->lsdb->hook_add = ospf6_top_lsdb_hook_add;
  o->lsdb->hook_remove = ospf6_top_lsdb_hook_remove;
 
  o->route_table = OSPF6_ROUTE_TABLE_CREATE(GLOBAL, ROUTES);
  o->route_table->scope = o;
  o->route_table->hook_add = ospf6_top_route_hook_add;
  o->route_table->hook_remove = ospf6_top_route_hook_remove;

  o->external_table = OSPF6_ROUTE_TABLE_CREATE(GLOBAL, ROUTES);
  o->external_table->scope = o;

  list_init(&o->area_list);
 
  o->restart_mode = my_restart_mode;

  o->checkpoint_enabled = true;  // default is for checkpointing to be enabled

  if(!o->restart_mode)
  {
    o->ready_to_checkpoint = true;
//    o->checkpoint_egress_xid = true;
  }
  else
  {
    o->ready_to_checkpoint = false;
//    o->checkpoint_egress_xid = false;
  }

  o->restarted_first_egress_not_sent = false;

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
      "interface IFNAME area A.B.C.D ctrl HOSTNUM",
      "Enable routing on an IPv6 interface\n"
      IFNAME_STR
      "Specify the OSPF6 area ID\n"
      "OSPF6 area ID in IPv4 address notation\n"
      "Controller to attach interface to\n")
{
  struct ospf6 *o;
  struct ospf6_area *oa;
  struct interface * ifp;
  struct ospf6_interface * oi;
  u_int32_t area_id;
  unsigned int hostnum;

  o = (struct ospf6 *)vty->index;

  hostnum = strtol(argv[2], NULL, 10);

  sibling_ctrl_add_ctrl_client(hostnum, argv[0], argv[1]);

  // abr
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

DEFUN(riack_host,
      riack_host_cmd,
      "riack host A.B.C.D",
      "Riack host address"
      V4NOTATION_STR)
{
  struct ospf6 * o;
  riack_client * riack_client = NULL;
  int port = 8087;
  int ret;

  o = (struct ospf6 *)vty->index;

  o->riack_client = db_init((char *)argv[0], port);

  return CMD_SUCCESS;
}

DEFUN(checkpointing,
      checkpointing_cmd,
      "checkpointing",
      "Enable checkpointing\n")
{
  struct ospf6 * o;

  o = (struct ospf6 *)vty->index;
  o->checkpoint_enabled = true;

  return CMD_SUCCESS;
}

DEFUN(no_checkpointing,
      no_checkpointing_cmd,
      "no checkpointing",
      "Disable checkpointing\n")
{
  struct ospf6 * o;

  o = (struct ospf6 *)vty->index;
  o->checkpoint_enabled = false;

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

void ospf6_change_restart_mode(bool restart_mode)
{
  struct ospf6 * o;

  o = (struct ospf6 *)ospf6;
  o->restart_mode = restart_mode;  
}

void ospf6_top_init(bool restart_mode)
{
  my_restart_mode =  restart_mode;

  /* Install ospf6 top node */
  install_node(&ospf6_node, config_write_ospf6);

  install_element(CONFIG_NODE, &router_ospf6_cmd);
 
  install_default(OSPF6_NODE);
  install_element(OSPF6_NODE, &ospf6_router_id_cmd);
  install_element(OSPF6_NODE, &ospf6_interface_area_cmd);
  install_element(OSPF6_NODE, &no_ospf6_interface_area_cmd);
  install_element(OSPF6_NODE, &riack_host_cmd);
  install_element(OSPF6_NODE, &checkpointing_cmd);
  install_element(OSPF6_NODE, &no_checkpointing_cmd);
}
