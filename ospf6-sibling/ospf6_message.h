#ifndef OSPF6_MESSAGE_H
#define OSPF6_MESSAGE_H

extern int ospf6_hello_send(struct thread * thread);
extern int ospf6_dbdesc_send (struct thread *thread);
extern int ospf6_receive(struct ctrl_client * ctrl_client, 
                         struct ospf6_header * oh,
                         unsigned int xid,
                         struct ospf6_interface * oi);

#endif
