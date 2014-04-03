#ifndef OSPF6_AREA_H
#define OSPF6_AREA_H

struct ospf6_area
{
  /* Area-ID */
  u_int32_t area_id;

  /* Area-ID string */
  char name[16];

  /* flag */
  u_char flag;

  /* Reference to Top data structure */
  struct ospf6 *ospf6;

  /* OSPF Option */
  u_char options[3];

  /* OSPF interface list */
  struct list if_list;

  struct ospf6_lsdb * lsdb;
  struct ospf6_lsdb * lsdb_self;

  struct ospf6_route_table *spf_table;

  struct thread * thread_spf_calculation;

  struct list node;
};

#define OSPF6_AREA_STUB       0x08

#define IS_AREA_STUB(oa) (CHECK_FLAG ((oa)->flag, OSPF6_AREA_STUB))

extern struct ospf6_area * ospf6_area_create (u_int32_t, struct ospf6 *);
extern struct ospf6_area * ospf6_area_lookup (u_int32_t, struct ospf6 *);
#endif
