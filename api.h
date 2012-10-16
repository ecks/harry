#ifndef API_H
#define API_H

void api_init();
int route_read(struct list * ipv4_rib_routes, struct list * ipv6_rib_routes);
int interface_list(struct list * list);

#endif
