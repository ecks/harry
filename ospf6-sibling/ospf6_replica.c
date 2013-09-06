#include "config.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>

#include "util.h"
#include "dblist.h"
#include "debug.h"
#include "prefix.h"
#include "sisis_api.h"
#include "sisis.h"
#include "sisis_process_types.h"
#include "routeflow-common.h"
#include "ospf6_replica.h"

static struct ospf6_replica * replica;

void ospf6_replicas_restart();

int rib_monitor_add_ipv4_route(struct route_ipv4 * route, void * data)
{
  if(IS_OSPF6_SIBLING_DEBUG_REPLICA)
  {
    char prefix_str[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &(route->p->prefix.s_addr), prefix_str, INET_ADDRSTRLEN) != 1)
      zlog_debug("Added route: %s/%d [%u/%u]", prefix_str, route->p->prefixlen, route->distance, route->metric);
  }                 

  // Free memory
  free(route);
}

int rib_monitor_remove_ipv4_route(struct route_ipv4 * route, void * data)
{
  if(IS_OSPF6_SIBLING_DEBUG_SISIS)
  {
    char prefix_str[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &(route->p->prefix.s_addr), prefix_str, INET_ADDRSTRLEN) != 1)
      zlog_debug("Removed route: %s/%d [%u/%u]", prefix_str, route->p->prefixlen, route->distance, route->metric);
  }

  // Free memory
  free(route);
}

int rib_monitor_add_ipv6_route(struct route_ipv6 * route, void * data)
{
  if(IS_OSPF6_SIBLING_DEBUG_REPLICA)
  {
    char prefix_str[INET6_ADDRSTRLEN];
    if (inet_ntop(AF_INET6, &(route->p->prefix.s6_addr), prefix_str, INET6_ADDRSTRLEN) != 1)
      zlog_debug("Added route: %s/%d [%u/%u]", prefix_str, route->p->prefixlen, route->distance, route->metric);
  }

  // Free memory
  free(route);
}

int rib_monitor_remove_ipv6_route(struct route_ipv6 * route, void * data)
{
  if(IS_OSPF6_SIBLING_DEBUG_REPLICA)
  {
    char prefix_str[INET6_ADDRSTRLEN];
    if (inet_ntop(AF_INET6, &(route->p->prefix.s6_addr), prefix_str, INET6_ADDRSTRLEN) != 1)
      zlog_debug("Removed route: %s/%d [%u/%u]", prefix_str, route->p->prefixlen, route->distance, route->metric);
  }
                    
  // Free memory
  free(route);

  if(replica->leader)
  {
    // try to restart the replica
    ospf6_replicas_restart();
  }
}

int ospf6_leader_elect()
{
  char sisis_addr[INET6_ADDRSTRLEN];
  unsigned int num_of_siblings = number_of_sisis_addrs_for_process_type(SISIS_PTYPE_OSPF6_SBLING);
  
  inet_ntop(AF_INET6, replica->sibling_addr, sisis_addr, INET6_ADDRSTRLEN);

  if(num_of_siblings == OSPF6_NUM_SIBS)
  {
    uint64_t own_prefix, own_sisis_version, own_process_type, own_process_version, own_sys_id, own_pid, own_ts; 
    if(get_sisis_addr_components(sisis_addr, 
                                 &own_prefix, 
                                 &own_sisis_version, 
                                 &own_process_type, 
                                 &own_process_version, 
                                 &own_sys_id, 
                                 &own_pid, 
                                 &own_ts) == 0)
    {
      bool oldest_sibling = true;
      struct list * sibling_addrs = get_ospf6_sibling_addrs();
      struct route_ipv6 * route_iter;
      LIST_FOR_EACH(route_iter, struct route_ipv6, node, sibling_addrs)
      {
        char addr[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &route_iter->p->prefix, addr, INET6_ADDRSTRLEN);

        if(memcmp(&route_iter->p->prefix, replica->sibling_addr, sizeof(struct in6_addr)) == 0)
        {
          // we came across our own timestamp
          continue;
        }
        else
        {
          uint64_t prefix, 
                   sisis_version, 
                   process_type, 
                   process_version, 
                   sys_id, 
                   pid, 
                   ts; 
          if(get_sisis_addr_components(addr, &prefix, &sisis_version, &process_type, &process_version, &sys_id, &pid, &ts) == 0)
          {
            if(ts < own_ts)
            {
              // this sibling does not have the smallest timestamp
              oldest_sibling = false;
              break;
            }
          }
        }
      }

      if(oldest_sibling)
      {
        if(IS_OSPF6_SIBLING_DEBUG_REPLICA)
        {
          zlog_debug("I am the oldest sibling");
          replica->leader = true;
        }
      }
      else
      {
        if(IS_OSPF6_SIBLING_DEBUG_REPLICA)
        {
          zlog_debug("I am not the oldest sibling");
          replica->leader = false;
        }
      }
    }
    else
    {
      // couldn't get components of own sisis addr
    }
  }
  else
  {
    printf("Not enough siblings\n");
    exit(1);
  }
}

void ospf6_replica_restart()
{
  // TODO: execute a qsub command
}

void ospf6_replicas_restart()
{  
  unsigned int num_of_siblings = number_of_sisis_addrs_for_process_type(SISIS_PTYPE_OSPF6_SBLING);
  unsigned int num_of_siblings_needed;
  int i;

  if(num_of_siblings < OSPF6_NUM_SIBS)
  {
    num_of_siblings_needed = OSPF6_NUM_SIBS - num_of_siblings;
    for(i = 0; i < num_of_siblings_needed; i++)
    {
      ospf6_replica_restart();
    }
  }
}

void ospf6_replica_init(struct in6_addr * own_sibling_addr)
{
  replica = calloc(1, sizeof(struct ospf6_replica));
  replica->sibling_addr = calloc(1 , sizeof(struct in6_addr));
  memcpy(replica->sibling_addr, own_sibling_addr, sizeof(struct in6_addr));
}
