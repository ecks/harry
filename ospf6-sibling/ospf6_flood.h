#ifndef OSPF6_FLOOD_H
#define OSPF6_FLOOD_H

/* Function Prototypes */
extern struct ospf6_lsdb *ospf6_get_scoped_lsdb (struct ospf6_lsa *lsa);

/* access method to retrans_count */
extern void ospf6_increment_retrans_count (struct ospf6_lsa *lsa);
extern void ospf6_decrement_retrans_count (struct ospf6_lsa *lsa);

#endif
