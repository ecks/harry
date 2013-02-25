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
//  int retval;
//  int i;
  int error;

  error = dp_new(&dp, dpid);
  add_controller(dp, name);
  dp_run(dp);

  zl_serv = zl_serv_new();
  zl_serv_init(zl_serv);


//  retval = vconn_open("tcp:0.0.0.0", RFP10_VERSION, &vconn, DSCP_DEFAULT);

//  vconn_run(vconn);

//  for(i = 0; i < 1; i++)
//  {
//    struct rfpbuf *buffer;
//    int error = vconn_recv(vconn, &buffer);
//    const struct rfp_header *rh = buffer->data;
//    switch (rh->type) {
//      case RFPT_FEATURES_REQUEST:
//       printf("Features request message received with xid: %d\n", ntohl(rh->xid));
//       make_routeflow_reply();:
//       break;
//      default:
//       break;
//    }
//  }
  
}

void listener_read(/* struct stream * stream, int stream_len */)
{
  int n;
  int s_len = sizeof(sin_other);

//  n = stream_recvfrom(stream, sockfd, stream_len, 0, (struct sockaddr *)&sin_other, &s_len);
//  printf("msg: %s\n", msg);
}
