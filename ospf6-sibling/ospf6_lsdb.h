#ifndef OSPF6_LSDB_H
#define OSPF6_LSDB_H

struct ospf6_lsdb
{
  u_int32_t count;
  void *data; /* data structure that holds this lsdb */
  struct route_table *table;
  void (*hook_add) (struct ospf6_lsa *);
  void (*hook_remove) (struct ospf6_lsa *);
};

struct ospf6_lsdb * ospf6_lsdb_create (void *data);
extern struct ospf6_lsa * ospf6_lsdb_head (struct ospf6_lsdb *lsdb);
extern struct ospf6_lsa *ospf6_lsdb_next (struct ospf6_lsa *lsa);
extern struct ospf6_lsa * ospf6_lsdb_type_router_head (u_int16_t type, u_int32_t adv_router, struct ospf6_lsdb *lsdb);
extern struct ospf6_lsa * ospf6_lsdb_type_router_next (u_int16_t type, u_int32_t adv_router, struct ospf6_lsa *lsa);
extern void ospf6_lsdb_add (struct ospf6_lsa *lsa, struct ospf6_lsdb *lsdb);
extern void ospf6_lsdb_remove_all (struct ospf6_lsdb *lsdb);
extern struct ospf6_lsa * ospf6_lsdb_lookup(u_int16_t type, u_int32_t id, u_int32_t adv_router, struct ospf6_lsdb * lsdb);
extern u_int32_t ospf6_new_ls_seqnum (u_int16_t type, u_int32_t id, u_int32_t adv_router, struct ospf6_lsdb *lsdb);
#endif
