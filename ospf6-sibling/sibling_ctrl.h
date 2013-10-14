#ifndef SIBLING_CTRL_H
#define SIBLING_CTRL_H

void sibling_ctrl_init(struct in6_addr * ctrl_addr, 
                       struct in6_addr * sibling_ctrl);
void sibling_ctrl_interface_init(char * interface);

#endif
