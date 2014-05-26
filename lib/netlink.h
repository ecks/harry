#ifndef NETLINK_H
#define NETLINK_H

struct netlink_routing_table_info
{
  int (*rib_add_ipv4_route)(struct route_ipv4 *, void *);
  int (*rib_remove_ipv4_route)(struct route_ipv4 *, void *);
  struct list * ipv4_rib;
  #ifdef HAVE_IPV6
  int (*rib_add_ipv6_route)(struct route_ipv6 *, void *);
  int (*rib_remove_ipv6_route)(struct route_ipv6 *, void *);
  struct list * ipv6_rib;
  #endif /* HAVE_IPV6 */
  struct netlink_wait_for_rib_changes_info * nl_info;
};

struct netlink_addrs_info
{
  struct list * ipv4_addrs;
  int (*add_ipv4_addr)(int, void *, u_char, struct list *);
  int (*remove_ipv4_addr)(int, struct list *);
  #ifdef HAVE_IPV6
  struct list * ipv6_addrs;
  int (*add_ipv6_addr)(int, void *, u_char, struct list *);
  int (*remove_ipv6_addr)(int, struct list *);
  #endif
};

struct netlink_port_info
{
  int (*add_port)(int index, unsigned int flags, unsigned int, char * name, struct list * list);
  struct list * all_ports;
};

int netlink_route_read(struct netlink_routing_table_info * info);
int netlink_addr_read(struct netlink_addrs_info * info);
int netlink_link_read(struct netlink_port_info * info);

/* Info for ntelink_wait_for_rib_changes */
struct netlink_wait_for_rib_changes_info
{
  struct nlsock * netlink_rib;
  struct netlink_routing_table_info * info;
};

extern int netlink_route_set(struct prefix * p, unsigned int ifindex, struct in6_addr * nexthop_addr);

/* Thread to wait for and process rib changes on a socket. */
void * netlink_wait_for_rib_changes(void *);

/* Subscribe to routing table using netlink interface. */
int netlink_subscribe_to_rib_changes(struct netlink_routing_table_info * info);

/* Unsubscribe to routing table using netlink interface. */
int netlink_unsubscribe_to_rib_changes(struct netlink_routing_table_info * info);

#endif
