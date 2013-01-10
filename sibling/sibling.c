#include "stdlib.h"
#include "stdint.h"
#include "stdbool.h"
#include "stdio.h"
#include "string.h"
#include "stddef.h"
#include "netinet/in.h"

#include "routeflow-common.h"
#include "util.h"
#include "dblist.h"
#include "prefix.h"
#include "socket-util.h"
#include "dblist.h"
#include "rfpbuf.h"
#include "rfp-msgs.h"
#include "vconn.h"
#include "thread.h"
#include "sisis.h"
#include "sisis_process_types.h"
#include "sibling_ctrl.h"

struct thread_master * master;

int main(int argc, char *argv[])
{
  struct vconn * vconn;
  int retval;
  struct rfpbuf * buffer;
  struct thread thread;

  int sisis_fd;
  uint64_t host_num = 1;

  /* thread master */
  master = thread_master_create();

  // TODO: signal_init

  if((sisis_fd = sisis_init(host_num, SISIS_PTYPE_SBLING) < 0))
  {
    printf("sisis_init error\n");
    exit(1);
  }

  unsigned int num_of_controllers = number_of_sisis_addrs_for_process_type(SISIS_PTYPE_CTRL);
  printf("num of controllers: %d\n", num_of_controllers);
 
  struct in6_addr * ctrl_addr = get_ctrl_addrs();
  char s_addr[INET6_ADDRSTRLEN+1];
  inet_ntop(AF_INET6, ctrl_addr, s_addr, INET6_ADDRSTRLEN+1);
  printf("done getting ctrl addr: %s\n", s_addr);

  sibling_ctrl_init(ctrl_addr);

  /* Start finite state machine, here we go! */
  while(thread_fetch(master, &thread))
    thread_call(&thread);

// NOT USED FOR NOW
/*
  retval = vconn_open("tcp:127.0.0.1:6634", RFP10_VERSION, &vconn, DSCP_DEFAULT);
  
  vconn_run(vconn);

  buffer = routeflow_alloc(RFPT_FEATURES_REQUEST, RFP10_VERSION, sizeof(struct rfp_header));
  retval = vconn_send(vconn, buffer); 

  retval = vconn_recv(vconn, &buffer);
  const struct rfp_header *rh = buffer->data;
  switch (rh->type) {
    case RFPT_FEATURES_REPLY:
      printf("Features reply message received\n");
      struct rfp_router_features * rrf = buffer->data;
      int i;
      int offset = offsetof(struct rfp_router_features, ports);
      size_t n_ports = ((ntohs(rrf->header.length)
                                         - offset)
                            / sizeof(*rrf->ports));
      printf("number of ports: %d\n", n_ports);
      for(i = 0; i < n_ports; i++)
      {  
        const struct rfp_phy_port * rpp = &rrf->ports[i];
        printf("port number: %d\n", ntohs(rpp->port_no));
      }
      break;
    default:
      break;
  } */
/*
  buffer = routeflow_alloc(RFPT_REDISTRIBUTE_REQUEST, RFP10_VERSION, sizeof(struct rfp_header));
  retval = vconn_send(vconn, buffer);

  while(1) 
  {
    retval = vconn_recv(vconn, &buffer);
    const struct rfp_header *rh = buffer->data;
    struct route_ipv4 * route = new_route();
    const struct rfp_ipv4_route * rir = buffer->data;

    switch (rh->type)
    {
      case RFPT_IPV4_ROUTE_ADD:

        route->p = new_prefix_v4();
        route->p->family = AF_INET;
        memcpy(&route->p->prefix, &rir->p, 4);
        route->p->prefixlen = ntohs(rir->prefixlen);

        // print route
        char prefix_str[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &(route->p->prefix.s_addr), prefix_str, INET_ADDRSTRLEN) != 1)
          printf("%s/%d\n", prefix_str, route->p->prefixlen);
        break;

      default:
        break;
    }
  } */
}
