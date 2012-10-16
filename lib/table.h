#ifndef TABLE_H
#define TABLE_H

/* Routing table top structure. */
struct route_table
{
  struct route_node *top;
};

/* Each routing entry. */
struct route_node
{
  /* Actual prefix of this radix. */
  struct prefix p;

  /* Tree link. */
  struct route_table *table;
  struct route_node *parent;
  struct route_node *link[2];
#define l_left   link[0]
#define l_right  link[1]

  /* Lock of this radix */
  unsigned int lock;

  /* Each node of route. */
  void *info;

  int valid;
  int label;

  /* Aggregation. */
  void *aggregate;
};

/* Prototypes. */
extern struct route_table *route_table_init ();
extern void route_table_finish (struct route_table *);
extern void route_unlock_node (struct route_node *node);
extern void route_node_delete (struct route_node *node);
extern void route_table_show (struct route_table * rt);
extern void route_node_show (struct route_node * rn);
extern struct route_node *route_top (struct route_table *);
extern struct route_node *route_next (struct route_node *);
extern struct route_node *route_next_until (struct route_node *,
                                            struct route_node *);
extern struct route_node *route_node_get (struct route_table *,
                                          struct prefix *);
extern struct route_node *route_node_lookup (struct route_table *,
                                             struct prefix *);
extern struct route_node *route_lock_node (struct route_node *node);
extern struct route_node *route_node_match (const struct route_table *,
                                            const struct prefix *);
extern struct route_node *route_node_match_ipv4 (const struct route_table *,
						 const struct in_addr *);
#endif /* TABLE_H */
