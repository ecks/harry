#ifndef OSPF6_TOP_H
#define OSPF6_TOP_H

/* cast macro: XXX - these *must* die, ick ick. */
#define OSPF6_PROCESS(x) ((struct ospf6 *) (x))
#define OSPF6_AREA(x) ((struct ospf6_area *) (x))
#define OSPF6_INTERFACE(x) ((struct ospf6_interface *) (x))
#define OSPF6_NEIGHBOR(x) ((struct ospf6_neighbor *) (x))

#ifndef timersub
#define timersub(a, b, res)                         \
  do {                                              \
    (res)->tv_sec = (a)->tv_sec - (b)->tv_sec;      \
    (res)->tv_usec = (a)->tv_usec - (b)->tv_usec;   \
    if ((res)->tv_usec < 0)                         \
    {                                               \
      (res)->tv_sec--;                              \
      (res)->tv_usec += 1000000;                    \
    }                                               \
  } while (0)
#endif /*timersub*/

/* top level data structure */
struct ospf6
{
  /* my router id */
  u_int32_t router_id;

  /* list of areas */
  struct list area_list;

  /* AS scope link state database */
  struct ospf6_lsdb *lsdb;

  struct ospf6_route_table *route_table;

  struct ospf6_route_table *external_table;

  riack_client * riack_client;

  bool restart_mode;

  bool ready_to_checkpoint;

  bool checkpoint_enabled;

  bool checkpoint_egress_xid;
};

/* global pointer for OSPF top data structure */
extern struct ospf6 *ospf6;

void ospf6_top_init(bool);
void ospf6_change_restart_mode(bool);

#endif
