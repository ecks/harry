#ifndef OSPF6_REPLICA
#define OSPF6_REPLICA

struct ospf6_replica
{
  struct in6_addr * sibling_addr;
  bool leader;
};

int rib_monitor_add_ipv4_route(struct route_ipv4 * route, void * data);
int rib_monitor_remove_ipv4_route(struct route_ipv4 * route, void * data);
int rib_monitor_add_ipv6_route(struct route_ipv6 * route, void * data);
int rib_monitor_remove_ipv6_route(struct route_ipv6 * route, void * data);
int ospf6_leader_elect();

#endif
