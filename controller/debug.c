#include <stdio.h>
#include <stdlib.h>

#include "vector.h"
#include "vty.h"
#include "command.h"
#include "debug.h"

unsigned long controller_debug_msg;

DEFUN(debug_controller_msg,
      debug_controller_msg_cmd,
      "debug controller msg",
      DEBUG_STR
      "Controller configuration\n"
      "Debug MSG events\n")
{
  SET_FLAG(controller_debug_msg, CONTROLLER_DEBUG_MSG);
  return CMD_SUCCESS;
}

void controller_debug_init(void)
{
  controller_debug_msg = 0;

  install_element(CONFIG_NODE, &debug_controller_msg_cmd);
}
