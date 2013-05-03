#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <unistd.h>

#include "vector.h"
#include "vty.h"
#include "command.h"
#include "debug.h"

unsigned long zebralite_debug_rib;

DEFUN(debug_zebralite_rib,
      debug_zebralite_rib_cmd,
      "debug zebralite rib",
      DEBUG_STR
      "Zebralite configuration\n"
      "Debug RIB events\n")
{
  SET_FLAG(zebralite_debug_rib, ZEBRALITE_DEBUG_RIB);
  return CMD_SUCCESS;
}

void zebra_debug_init(void)
{
  zebralite_debug_rib = 0;

  install_element(CONFIG_NODE, &debug_zebralite_rib_cmd);
}
