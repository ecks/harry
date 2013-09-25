#ifndef OSPF6_REPLICA
#define OSPF6_REPLICA

struct sibling
{
  struct in6_addr * sibling_addr;
  // right now, only our own sibling can be a leader, 
  // would need to modify the algorithm otherwise
  bool leader;
  bool valid; // when sibling is not valid, it needs to be deleted
  unsigned int sock;
  unsigned int id;
  struct list node;
};

struct ospf6_replica
{
  unsigned int sock;
  struct rfpbuf * ibuf;
  struct rfpbuf * obuf;
  int fail;
  struct thread * t_read;
  struct sibling * own_replica;
  // canonically, our own sibling should always be in the beginning of the list
  struct list replicas;
};

int rib_monitor_add_ipv4_route(struct route_ipv4 * route, void * data);
int rib_monitor_remove_ipv4_route(struct route_ipv4 * route, void * data);
int rib_monitor_add_ipv6_route(struct route_ipv6 * route, void * data);
int rib_monitor_remove_ipv6_route(struct route_ipv6 * route, void * data);
int ospf6_leader_elect();
void ospf6_replicas_init(struct in6_addr * own_replica_addr, struct list * replicas);

#endif
