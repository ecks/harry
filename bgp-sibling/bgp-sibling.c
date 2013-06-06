#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <sys/select.h>
#include <netinet/in.h>

#include "util.h"
#include "dblist.h"
#include "routeflow-common.h"
#include "if.h"
#include "thread.h"
#include "prefix.h"
#include "sisis.h"
#include "sisis_process_types.h"

struct thread_master * master;

int main(int argc, char * argv[])
{
  int sisis_fd;
  uint64_t host_num = 1;

  struct thread thread;

  /* thread master */
  master = thread_master_create();

  // initialize interface list
  if_init();

  if((sisis_fd = sisis_init(host_num, SISIS_PTYPE_BGP_SBLING)) < 0)
  {
    printf("sisis_init error\n");
    exit(1);
  }

  struct list * ctrl_addrs = get_ctrl_addrs();
  struct route_ipv6 * route_iter;
  LIST_FOR_EACH(route_iter, struct route_ipv6, node, ctrl_addrs)
  {
    char s_addr[INET6_ADDRSTRLEN+1];
    inet_ntop(AF_INET6, &route_iter->p->prefix, s_addr, INET6_ADDRSTRLEN+1);
    printf("done getting ctrl addr: %s\n", s_addr);

    struct in6_addr * ctrl_addr = calloc(1, sizeof(struct in6_addr));
    memcpy(ctrl_addr, &route_iter->p->prefix, sizeof(struct in6_addr));
    bgp_sibling_ctrl_init(ctrl_addr);
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

  /* Start finite state machine, here we go! */
  while(thread_fetch(master, &thread))
    thread_call(&thread);
}
