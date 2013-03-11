#ifndef OSPF6_INTERFACE_H
#define OSPF6_INTERFACE_H

struct ospf6_interface
{
  struct interface * interface;

  u_int16_t hello_interval;

  /* Interface state */
  u_char state;
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
int interface_up(struct thread * thread);

#endif
