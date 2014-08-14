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

int sisis_init(char * sisis_addr, uint64_t host_num, uint64_t ptype)
{
  // not used for now
  int ret = 0;

  sisis_register_host(sisis_addr, host_num, ptype, PTYPE_VERSION);

  kernel_init();

  return ret;
}

struct list * get_ctrl_addrs(void)
{
  char ctrl_addr[INET6_ADDRSTRLEN+1];
  sisis_create_addr(ctrl_addr, (uint64_t)SISIS_PTYPE_CTRL, (uint64_t)0, (uint64_t)0, (uint64_t)0, (uint64_t)0);
  struct prefix_ipv6 ctrl_prefix = sisis_make_ipv6_prefix(ctrl_addr, 37);
  struct list * ctrl_addrs = get_sisis_addrs_for_prefix(&ctrl_prefix);
  if(!list_empty(ctrl_addrs))
  {
    return ctrl_addrs;
  }
  return NULL; 
}

struct list * get_ctrl_addrs_for_hostnum(unsigned int hostnum)
{
  char ctrl_addr[INET6_ADDRSTRLEN+1];
  sisis_create_addr(ctrl_addr, (uint64_t)SISIS_PTYPE_CTRL, (uint64_t)PTYPE_VERSION, (uint64_t)hostnum, (uint64_t)0, (uint64_t)0);
  struct prefix_ipv6 ctrl_prefix = sisis_make_ipv6_prefix(ctrl_addr, 74);
  struct list * ctrl_addrs = get_sisis_addrs_for_prefix(&ctrl_prefix);
  
  return ctrl_addrs;
}

struct list * get_ospf6_sibling_addrs(void)
{
  char os_sib_addr[INET6_ADDRSTRLEN+1];
  sisis_create_addr(os_sib_addr, 
                    (uint64_t)SISIS_PTYPE_OSPF6_SBLING, 
                    (uint64_t)0, 
                    (uint64_t)0, 
                    (uint64_t)0, 
                    (uint64_t)0);
  struct prefix_ipv6 os_sib_prefix = sisis_make_ipv6_prefix(os_sib_addr, 37);
  struct list * os_sib_addrs = get_sisis_addrs_for_prefix(&os_sib_prefix);
  if(!list_empty(os_sib_addrs))
  {
    return os_sib_addrs;
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
