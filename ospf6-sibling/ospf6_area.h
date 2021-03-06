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

  struct ospf6_route_table *range_table;

  /* OSPF interface list */
  struct list if_list;

  struct ospf6_lsdb * lsdb;
  struct ospf6_lsdb * lsdb_self;

  struct ospf6_route_table * spf_table;

  struct ospf6_route_table * route_table;

  struct thread * thread_spf_calculation;

  struct thread * thread_router_lsa;
  u_int32_t router_lsa_size_limit;

  struct list node;
};

struct ospf6_area_hostnum
{
  struct ospf6_area * oa;
  unsigned int hostnum;
};

#define OSPF6_AREA_ENABLE     0x01
#define OSPF6_AREA_STUB       0x08

#define IS_AREA_ENABLED(oa) (CHECK_FLAG ((oa)->flag, OSPF6_AREA_ENABLE))
#define IS_AREA_STUB(oa) (CHECK_FLAG ((oa)->flag, OSPF6_AREA_STUB))

#define OSPF6_AREA_ID_STR   "Area ID (as an IPv4 notation)\n"

extern struct ospf6_area * ospf6_area_create (u_int32_t, struct ospf6 *);
extern struct ospf6_area * ospf6_area_lookup (u_int32_t, struct ospf6 *);
#endif
