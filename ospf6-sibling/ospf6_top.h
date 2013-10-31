#ifndef OSPF6_TOP_H
#define OSPF6_TOP_H

/* cast macro: XXX - these *must* die, ick ick. */
#define OSPF6_PROCESS(x) ((struct ospf6 *) (x))
#define OSPF6_AREA(x) ((struct ospf6_area *) (x))
#define OSPF6_INTERFACE(x) ((struct ospf6_interface *) (x))
#define OSPF6_NEIGHBOR(x) ((struct ospf6_neighbor *) (x))

/* top level data structure */
struct ospf6
{
  /* my router id */
  u_int32_t router_id;

  /* list of areas */
  struct list area_list;

  /* AS scope link state database */
  struct ospf6_lsdb *lsdb;
};

/* global pointer for OSPF top data structure */
extern struct ospf6 *ospf6;

void ospf6_top_init();

#endif
