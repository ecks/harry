#ifndef NETLINK_H
#define NETLINK_H

struct netlink_routing_table_info
{
  int (*rib_add_ipv4_route)(struct route_ipv4 *, struct list * list);
  int (*rib_remove_ipv4_route)(struct route_ipv4 *, struct list * list);
  struct list * ipv4_rib;
  #ifdef HAVE_IPV6
  int (*rib_add_ipv6_route)(struct route_ipv6 *, struct list * list);
  int (*rib_remove_ipv6_route)(struct route_ipv6 *, struct list * list);
  struct list * ipv6_rib;
  #endif /* HAVE_IPV6 */
};

struct netlink_port_info
{
  int (*add_port)();
  struct list * all_ports;
};

int netlink_route_read(struct netlink_routing_table_info * info);
int netlink_link_read(struct netlink_port_info * info);

#endif
