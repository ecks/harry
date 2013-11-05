#ifndef OSPF6_INTERFACE_H
#define OSPF6_INTERFACE_H

struct ospf6_interface
{
  struct interface * interface;

  /* back pointer */
  struct ospf6_area *area;
 
  /* list of ospf6 neighbors */
  struct list neighbor_list;

  /* linklocal address of this I/F */
  struct in6_addr *linklocal_addr;

  /* I/F transmission delay */
  u_int32_t transdelay;

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

  /* I/F MTU */
  u_int32_t ifmtu;

  /* Interface state */
  u_char state;

  /* Linklocal LSA Database: includes Link-LSA */
  struct ospf6_lsdb * lsdb;;
  struct ospf6_lsdb * lsdb_self;

  struct ospf6_lsdb * lsupdate_list;
  struct ospf6_lsdb * lsack_list;

  /* Ongoing tasks */
  struct thread *thread_send_hello;

  struct thread * thread_link_lsa;

  struct ospf6_route_table * route_connected;
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

extern struct ospf6_interface * ospf6_interface_create (struct interface *ifp);
extern void ospf6_interface_if_add(struct interface * ifp, struct ctrl_client * ctrl_client);
extern int interface_up(struct thread * thread);
extern int wait_timer(struct thread * thread);
extern int neighbor_change(struct thread * thread);

#endif
