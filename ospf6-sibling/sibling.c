#include "config.h"

#include <stdlib.h>
#include <unistd.h>
#include "stdint.h"
#include "stdbool.h"
#include "stdio.h"
#include "string.h"
#include "stddef.h"
#include "netinet/in.h"

#include "routeflow-common.h"
#include "util.h"
#include "dblist.h"
#include "if.h"
#include "prefix.h"
#include "socket-util.h"
#include "dblist.h"
#include "rfpbuf.h"
#include "rfp-msgs.h"
#include "vconn.h"
#include "thread.h"
#include "ospf6_top.h"
#include "sisis.h"
#include "sisis_process_types.h"
#include "sibling_ctrl.h"
#include "sibling.h"

struct thread_master * master;

int main(int argc, char *argv[])
{
  struct vconn * vconn;
  int retval;
  struct rfpbuf * buffer;
  struct thread thread;

  int sisis_fd;
  uint64_t host_num = 1;

  if(optind !=  argc-1)
  {
    printf("usage: sibling <interface>\n");
    exit(1);
  }
  /* thread master */
  master = thread_master_create();

  interface_name = argv[optind];

  // TODO: signal_init

  // initialize our interface list
  if_init();

  if((sisis_fd = sisis_init(host_num, SISIS_PTYPE_SBLING) < 0))
  {
    printf("sisis_init error\n");
    exit(1);
  }

  unsigned int num_of_controllers = number_of_sisis_addrs_for_process_type(SISIS_PTYPE_CTRL);
  printf("num of controllers: %d\n", num_of_controllers);
 
  struct list * ctrl_addrs = get_ctrl_addrs();
  struct route_ipv6 * route_iter;
  LIST_FOR_EACH(route_iter, struct route_ipv6, node, ctrl_addrs)
  {
    
    char s_addr[INET6_ADDRSTRLEN+1];
    inet_ntop(AF_INET6, &route_iter->p->prefix, s_addr, INET6_ADDRSTRLEN+1);
    printf("done getting ctrl addr: %s\n", s_addr);

    struct in6_addr * ctrl_addr = calloc(1, sizeof(struct in6_addr));
    memcpy(ctrl_addr, &route_iter->p->prefix, sizeof(struct in6_addr));
    sibling_ctrl_init(ctrl_addr);
  }

  // free  the list, no longer needed
  while(!list_empty(ctrl_addrs))
  {
    struct list * addr_to_remove = list_pop_front(ctrl_addrs);
    struct route_ipv6 * route_to_remove = CONTAINER_OF(addr_to_remove, struct route_ipv6, node);
    free(route_to_remove->p);
    free(route_to_remove->gate);
  }

  free(ctrl_addrs);
  
  ospf6_top_init(interface_name);

  /* Start finite state machine, here we go! */
  while(thread_fetch(master, &thread))
    thread_call(&thread);

}
