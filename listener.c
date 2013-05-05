#include <stdio.h>
#include <stdlib.h> 
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "lib/routeflow-common.h"
#include "lib/dblist.h"
#include "vector.h"
#include "vty.h"
#include "command.h"
#include "lib/rfpbuf.h"
#include "thread.h"
#include "lib/vconn.h"
#include "datapath.h"
#include "zl_serv.h"
#include "listener.h"
#include "lib/socket-util.h"

static struct datapath * dp;
static struct zl_serv * zl_serv;
static uint64_t dpid = UINT64_MAX;
static char * name;

DEFUN(controller,
    controller_cmd,
    "controller CONN",
    "control command\n"
    "protocol and ip address of controller\n")
{
  name = calloc(1, sizeof(char) * strlen(argv[0]));
  strncpy(name, argv[0], strlen(argv[0]));
  return CMD_SUCCESS;
}

void listener_init()
{
  install_element(CONFIG_NODE, &controller_cmd); 
}

void listener_run()
{
  int error;

  dp = dp_new(dpid);
  add_controller(dp, name);
  dp_run(dp);

  zl_serv = zl_serv_new();
  zl_serv_init(zl_serv, dp);
}
