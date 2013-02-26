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
#include "lib/rfpbuf.h"
#include "lib/vconn.h"
#include "datapath.h"
#include "zl_serv.h"
#include "listener.h"
#include "lib/socket-util.h"

#define PORT 9999
#define MSGLEN 1024

int sockfd;
struct sockaddr_in sin_me, sin_other;
char msg[MSGLEN];

static struct datapath * dp;
static struct zl_serv * zl_serv;
static uint64_t dpid = UINT64_MAX;

void listener_init(const char * name)
{
  int error;

  error = dp_new(&dp, dpid);
  add_controller(dp, name);
  dp_run(dp);

  zl_serv = zl_serv_new();
  zl_serv_init(zl_serv, dp);
}
