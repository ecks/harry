#ifndef OSPF6_FLOOD_H
#define OSPF6_FLOOD_H

/* Function Prototypes */
extern struct ospf6_lsdb *ospf6_get_scoped_lsdb (struct ospf6_lsa *lsa);
extern struct ospf6_lsdb *ospf6_get_scoped_lsdb_self (struct ospf6_lsa *lsa);

/* origination & purging */
extern void ospf6_lsa_originate (struct ospf6_lsa *lsa);
//extern void ospf6_lsa_originate_process (struct ospf6_lsa *lsa,
//                                             struct ospf6 *process);
extern void ospf6_lsa_originate_area (struct ospf6_lsa *lsa,
                                         struct ospf6_area *oa);
extern void ospf6_lsa_originate_interface (struct ospf6_lsa *lsa,
                                               struct ospf6_interface *oi);

/* access method to retrans_count */
extern void ospf6_increment_retrans_count (struct ospf6_lsa *lsa);
extern void ospf6_decrement_retrans_count (struct ospf6_lsa *lsa);

/* flooding & clear flooding */
extern void ospf6_flood_clear (struct ospf6_lsa *lsa);
extern void ospf6_flood (struct ospf6_neighbor *from, struct ospf6_lsa *lsa);

/* receive & install */
extern void ospf6_install_lsa (struct ospf6_lsa *lsa);
extern void ospf6_lsa_purge(struct ospf6_lsa * lsa);
extern void ospf6_receive_lsa(struct ospf6_neighbor * from, struct ospf6_lsa_header * lsa_header, unsigned int hostnum);
#endif
