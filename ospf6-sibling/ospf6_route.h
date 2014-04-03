#ifndef OSPF6_ROUTE_H
#define OSPF6_ROUTE_H

#define OSPF6_MULTI_PATH_LIMIT    4

/* Nexthop */
struct ospf6_nexthop
{
  /* Interface index */
  unsigned int ifindex;

  /* IP address, if any */
  struct in6_addr address;
};

#define ospf6_nexthop_is_set(x)                                \
  ((x)->ifindex || ! IN6_IS_ADDR_UNSPECIFIED (&(x)->address))
#define ospf6_nexthop_is_same(a,b)                             \
  ((a)->ifindex == (b)->ifindex &&                            \
  IN6_ARE_ADDR_EQUAL (&(a)->address, &(b)->address))
#define ospf6_nexthop_clear(x)                                \
  do {                                                        \
    (x)->ifindex = 0;                                         \
    memset (&(x)->address, 0, sizeof (struct in6_addr));      \
  } while (0)
#define ospf6_nexthop_copy(a, b)                              \
  do {                                                        \
    (a)->ifindex = (b)->ifindex;                              \
    memcpy (&(a)->address, &(b)->address,                     \
            sizeof (struct in6_addr));                        \
  } while (0)

/* Path */
struct ospf6_ls_origin
{
  u_int16_t type;
  u_int32_t id; 
  u_int32_t adv_router;
};

#define OSPF6_PATH_TYPE_NONE         0
#define OSPF6_PATH_TYPE_INTRA        1
#define OSPF6_PATH_TYPE_INTER        2
#define OSPF6_PATH_TYPE_EXTERNAL1    3
#define OSPF6_PATH_TYPE_EXTERNAL2    4
#define OSPF6_PATH_TYPE_REDISTRIBUTE 5
#define OSPF6_PATH_TYPE_MAX          6

struct ospf6_path
{
  /* Link State Origin */
  struct ospf6_ls_origin origin;

  /* Router bits */
  u_char router_bits;

  /* Prefix Options */
  u_char prefix_options;

  /* Optional Capabilities */
  u_char options[3];

  /* Path-type */
  u_char type;

  /* Cost */
  u_int8_t metric_type;
  u_int32_t cost;
  u_int32_t cost_e2;
};

struct ospf6_route
{
  struct route_node * rnode;
  struct ospf6_route_table * table;
  struct ospf6_route * prev;
  struct ospf6_route * next;

  unsigned int lock;

  /* Destination Type */
  u_char type;

  /* Destination ID */
  struct prefix prefix;

  /* Time */
  struct timeval installed;
  struct timeval changed;

  /* flag */
  u_char flag;

  /* path */
  struct ospf6_path path;

  /* nexthop */
  struct ospf6_nexthop nexthop[OSPF6_MULTI_PATH_LIMIT];

  /* route option */
  void * route_option;

  /* link state id for advertising */
  u_int32_t linkstate_id;
};

#define OSPF6_DEST_TYPE_NETWORK    2
#define OSPF6_DEST_TYPE_LINKSTATE  4

#define OSPF6_ROUTE_CHANGE           0x01
#define OSPF6_ROUTE_ADD              0x02
#define OSPF6_ROUTE_BEST             0x08
#define OSPF6_ROUTE_WAS_REMOVED      0x40

struct ospf6_route_table
{
  int scope_type;
  int table_type;

  /* patricia tree */
  struct route_table * table;
  void *scope;

  u_int32_t count;

  /* hooks */
  void (*hook_add) (struct ospf6_route *);
  void (*hook_change) (struct ospf6_route *);
  void (*hook_remove) (struct ospf6_route *);
};

#define OSPF6_SCOPE_TYPE_AREA      2
#define OSPF6_SCOPE_TYPE_INTERFACE 3

#define OSPF6_TABLE_TYPE_NONE              0
#define OSPF6_TABLE_TYPE_ROUTES            1
#define OSPF6_TABLE_TYPE_CONNECTED_ROUTES  3

#define OSPF6_ROUTE_TABLE_CREATE(s, t) \
  ospf6_route_table_create (OSPF6_SCOPE_TYPE_ ## s, \
                            OSPF6_TABLE_TYPE_ ## t)

#define ospf6_route_is_same(ra, rb) \
    (prefix_same (&(ra)->prefix, &(rb)->prefix))
#define ospf6_route_is_same_origin(ra, rb) \
        (memcmp (&(ra)->path.origin, &(rb)->path.origin, \
                     sizeof (struct ospf6_ls_origin)) == 0)
//    ((ra)->path.area_id == (rb)->path.area_id && 
#define ospf6_route_is_identical(ra, rb) \
    ((ra)->type == (rb)->type && \
        memcmp (&(ra)->prefix, &(rb)->prefix, sizeof (struct prefix)) == 0 && \
        memcmp (&(ra)->path, &(rb)->path, sizeof (struct ospf6_path)) == 0 && \
        memcmp (&(ra)->nexthop, &(rb)->nexthop,                               \
                     sizeof (struct ospf6_nexthop) * OSPF6_MULTI_PATH_LIMIT) == 0)

#define ospf6_linkstate_prefix_adv_router(x) \
    (*(u_int32_t *)(&(x)->u.prefix6.s6_addr[0]))
#define ospf6_linkstate_prefix_id(x) \
    (*(u_int32_t *)(&(x)->u.prefix6.s6_addr[4]))

extern void ospf6_linkstate_prefix (u_int32_t adv_router, u_int32_t id, struct prefix *prefix);
extern struct ospf6_route * ospf6_route_create(void);
extern struct ospf6_route * ospf6_route_lookup (struct prefix *prefix,
                                                struct ospf6_route_table *table);
extern struct ospf6_route * ospf6_route_add(struct ospf6_route * route, 
                                            struct ospf6_route_table * table);
extern void ospf6_route_remove (struct ospf6_route *route,
                                struct ospf6_route_table *table);
extern struct ospf6_route *ospf6_route_head (struct ospf6_route_table *table);
extern struct ospf6_route *ospf6_route_next (struct ospf6_route *route);

extern void ospf6_route_remove_all (struct ospf6_route_table *);
extern struct ospf6_route_table *ospf6_route_table_create (int s, int t);
#endif
