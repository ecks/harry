#ifndef SIBLING_CTRL_H
#define SIBLING_CTRL_H

void sibling_ctrl_init(struct in6_addr * ctrl_addr, 
                       struct in6_addr * sibling_ctrl);
void sibling_ctrl_interface_init(char * interface);

int sibling_ctrl_first_xid_rcvd();

struct list * sibling_ctrl_restart_msg_queue();
#endif
