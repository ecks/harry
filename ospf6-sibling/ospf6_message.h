#ifndef OSPF6_MESSAGE_H
#define OSPF6_MESSAGE_H

extern int ospf6_hello_send(struct thread * thread);
extern int ospf6_dbdesc_send (struct thread *thread);
extern int ospf6_receive(struct ctrl_client * ctrl_client, 
                         struct rfp_header* rh, 
                         struct ospf6_interface * oi);

#endif
