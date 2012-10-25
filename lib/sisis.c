#include "config.h"

#include "stdint.h"
#include "stdlib.h"
#include "stdbool.h"
#include "stddef.h"
#include "pthread.h"
#include "netinet/in.h"

#include "util.h"
#include "dblist.h"
#include "prefix.h"
#include "sisis_structs.h"
#include "sisis_api.h"
#include "netlink.h"
#include "sisis_process_types.h"

int sisis_init(uint64_t host_num, uint64_t ptype)
{
  // not used for now
  int sockfd = 0;

  sisis_register_host(host_num, ptype, SISIS_VERSION);

  kernel_init();

  return sockfd;
}

struct in6_addr * get_ctrl_addr(void)
{
  char ctrl_addr[INET6_ADDRSTRLEN+1];
  sisis_create_addr(ctrl_addr, (uint64_t)SISIS_PTYPE_CTRL, (uint64_t)0, (uint64_t)0, (uint64_t)0, (uint64_t)0);
  struct prefix_ipv6 ctrl_prefix = sisis_make_ipv6_prefix(ctrl_addr, 37);
  struct list * ctrl_addrs = get_sisis_addrs_for_prefix(&ctrl_prefix);
  if(list_size(ctrl_addrs) == 1)
  {
    struct route_ipv6 * route = CONTAINER_OF(list_pop_front(ctrl_addrs), struct route_ipv6, node); 
    return (struct in6_addr *)&route->p->prefix;
  }
  return NULL; 
}

unsigned int number_of_sisis_addrs_for_process_type (unsigned int ptype)
{
  char addr[INET6_ADDRSTRLEN+1];
  unsigned int lsize;

  sisis_create_addr(addr, (uint64_t)ptype, (uint64_t)0, (uint64_t)0, (uint64_t)0, (uint64_t)0);
  struct prefix_ipv6 prefix = sisis_make_ipv6_prefix(addr, 37);
  struct list * addrs = get_sisis_addrs_for_prefix(&prefix);

  lsize = list_size(addrs);

//  FREE_LINKED_LIST (addrs); be careful about freeing data
  return lsize;
}
