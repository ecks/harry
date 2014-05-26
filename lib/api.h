#ifndef API_H
#define API_H

void api_init();
int routes_list(struct list * ipv4_rib_routes, struct list * ipv6_rib_routes, int (*rib_add_ipv4)(struct route_ipv4 *, void *), int (*rib_add_ipv6)(struct route_ipv6 *, void *));
int addrs_list(struct list *, struct list *, int (*add_ipv4_addr)(int index, void * address, u_char prefixlen, struct list * list),
                                             int (*add_ipv6_addr)(int index, void * address, u_char prefixlen, struct list * list));
int interface_list(struct list * list, int (*add_port)(int index, unsigned int flags, unsigned int mtu, char * name, struct list * list));
int install_route_v6(struct route_ipv6 * route, struct in6_addr * nexthop_addr);

#endif
