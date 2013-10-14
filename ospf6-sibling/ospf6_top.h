#ifndef OSPF6_TOP_H
#define OSPF6_TOP_H

/* top level data structure */
struct ospf6
{
  /* my router id */
  u_int32_t router_id;

  /* AS scope link state database */
  struct ospf6_lsdb *lsdb;
};

/* global pointer for OSPF top data structure */
extern struct ospf6 *ospf6;

void ospf6_top_init();

#endif
