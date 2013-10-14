#include <stdio.h>
#include <stdlib.h>

#include "vector.h"
#include "vty.h"
#include "command.h"
#include "debug.h"

unsigned long ospf6_sibling_debug_msg;
unsigned long ospf6_sibling_debug_sisis;
unsigned long ospf6_sibling_debug_replica;
unsigned long ospf6_sibling_debug_neighbor;
unsigned long ospf6_sibling_debug_ctrl_client;
unsigned long ospf6_sibling_debug_interface;

DEFUN(debug_ospf6_sibling_msg,
      debug_ospf6_sibling_msg_cmd,
      "debug ospf6 sibling msg",
      DEBUG_STR
      "OSPF6 Sibling configuration\n"
      "Debug MSG events\n")
{
  SET_FLAG(ospf6_sibling_debug_msg, OSPF6_SIBLING_DEBUG_MSG);
  return CMD_SUCCESS;
}

DEFUN(debug_ospf6_sibling_sisis,
      debug_ospf6_sibling_sisis_cmd,
      "debug ospf6 sibling sisis",
      DEBUG_STR
      "OSPF6 Sibling configuration\n"
      "Debug MSG events\n")
{
  SET_FLAG(ospf6_sibling_debug_sisis, OSPF6_SIBLING_DEBUG_SISIS);
  return CMD_SUCCESS;
}

DEFUN(debug_ospf6_sibling_replica,
      debug_ospf6_sibling_replica_cmd,
      "debug ospf6 sibling replica",
      DEBUG_STR
      "OSPF6 Sibling configuration\n"
      "Debug MSG events\n")
{
  SET_FLAG(ospf6_sibling_debug_replica, OSPF6_SIBLING_DEBUG_REPLICA);
  return CMD_SUCCESS;
}

DEFUN(debug_ospf6_sibling_neighbor,
      debug_ospf6_sibling_neighbor_cmd,
      "debug ospf6 sibling neighbor",
      DEBUG_STR
      "OSPF6 Sibling configuration\n"
      "Debug MSG events\n")
{
  SET_FLAG(ospf6_sibling_debug_neighbor, OSPF6_SIBLING_DEBUG_NEIGHBOR);
  return CMD_SUCCESS;
}

DEFUN(debug_ospf6_sibling_ctrl_client,
      debug_ospf6_sibling_ctrl_client_cmd,
      "debug ospf6 sibling ctrl client",
      DEBUG_STR
      "OSPF6 Sibling configuration\n"
      "Debug MSG events\n")
{
  SET_FLAG(ospf6_sibling_debug_ctrl_client, OSPF6_SIBLING_DEBUG_CTRL_CLIENT);
  return CMD_SUCCESS;
}

DEFUN(debug_ospf6_sibling_intreface,
      debug_ospf6_sibling_interface_cmd,
      "debug ospf6 sibling interface",
      DEBUG_STR
      "OSPF6 Sibling configuration\n"
      "Debug MSG events\n")
{
  SET_FLAG(ospf6_sibling_debug_interface, OSPF6_SIBLING_DEBUG_INTERFACE);
  return CMD_SUCCESS;
}

void ospf6_sibling_debug_init()
{
  ospf6_sibling_debug_msg = 0;

  install_element(CONFIG_NODE, &debug_ospf6_sibling_msg_cmd);

  install_element(CONFIG_NODE, &debug_ospf6_sibling_sisis_cmd);
  
  install_element(CONFIG_NODE, &debug_ospf6_sibling_replica_cmd);

  install_element(CONFIG_NODE, &debug_ospf6_sibling_neighbor_cmd);

  install_element(CONFIG_NODE, &debug_ospf6_sibling_ctrl_client_cmd);

  install_element(CONFIG_NODE, &debug_ospf6_sibling_interface_cmd);
}
