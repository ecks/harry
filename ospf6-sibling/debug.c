#include <stdio.h>
#include <stdlib.h>

#include "vector.h"
#include "vty.h"
#include "command.h"
#include "debug.h"

unsigned long ospf6_sibling_debug_msg;

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

void ospf6_sibling_debug_init()
{
  ospf6_sibling_debug_msg = 0;

  install_element(CONFIG_NODE, &debug_ospf6_sibling_msg_cmd);
}
