#ifndef SIBLING_CTRL_H
#define SIBLING_CTRL_H

void sibling_ctrl_init();
timestamp_t sibling_ctrl_ingress_timestamp();

void sibling_ctrl_add_ctrl_client(struct in6_addr * ctrl_addr,
                                  struct in6_addr * sibling_addr);
void sibling_ctrl_interface_init(struct ospf6_interface * oi);

void sibling_ctrl_route_set(struct ospf6_route * route);

int sibling_ctrl_first_xid_rcvd();

// functions dealing with restart msg queue
timestamp_t sibling_ctrl_first_timestamp_rcvd();
struct list * sibling_ctrl_restart_msg_queue();
int sibling_ctrl_push_to_restart_msg_queue(struct rfpbuf * msg_rcvd);

#endif
