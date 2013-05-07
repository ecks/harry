#ifndef OSPF6_INTERFACE_H
#define OSPF6_INTERFACE_H

struct ospf6_interface
{
  struct interface * interface;

  /* back pointer */
  struct ospf6_area *area;
  
  struct ctrl_client * ctrl_client;

  /* Router Priority */
  u_char priority;

  /* Time Interval */
  u_int16_t hello_interval;
  u_int16_t dead_interval;
  u_int32_t rxmt_interval;

  /* Decision of DR Election */
  u_int32_t drouter;
  u_int32_t bdrouter;

  /* Interface state */
  u_char state;

  /* Ongoing tasks */
  struct thread *thread_send_hello;
};

/* interface state */
#define OSPF6_INTERFACE_NONE             0
#define OSPF6_INTERFACE_DOWN             1
#define OSPF6_INTERFACE_LOOPBACK         2
#define OSPF6_INTERFACE_WAITING          3
#define OSPF6_INTERFACE_POINTTOPOINT     4
#define OSPF6_INTERFACE_DROTHER          5
#define OSPF6_INTERFACE_BDR              6
#define OSPF6_INTERFACE_DR               7
#define OSPF6_INTERFACE_MAX              8

struct ospf6_interface * ospf6_interface_create (struct interface *ifp);
void ospf6_interface_if_add(struct interface * ifp, struct ctrl_client * ctrl_client);
int interface_up(struct thread * thread);

#endif
