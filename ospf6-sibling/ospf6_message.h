#ifndef OSPF6_MESSAGE_H
#define OSPF6_MESSAGE_H

int ospf6_hello_send(struct thread * thread);
int ospf6_receive(struct rfp_header* rh, struct ospf6_interface * oi);

#endif
