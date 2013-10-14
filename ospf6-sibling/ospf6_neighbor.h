#ifndef OSPF6_NEIGHBOR_H
#define OSPF6_NEIGHBOR_H

/* Neighbor structure */
struct ospf6_neighbor
{
  /* Neighbor Router ID String */
  char name[32];

  /* OSPFv3 Interface this neighbor belongs to */
  struct ospf6_interface *ospf6_if;

  struct list node;
  u_char state;

  /* timestamp of last changing state */
  struct timeval last_changed;

  u_int32_t router_id;

  /* Neighbor Interface ID */
  u_int32_t ifindex;

  /* Router Priority of this neighbor */
  u_char priority;

  u_int32_t drouter;
  u_int32_t bdrouter;
  u_int32_t prev_drouter;
  u_int32_t prev_bdrouter;

  /* Options field (Capability) */
  char options[3];

  /* For Database Exchange */
  u_char dbdesc_bits;
  u_int32_t dbdesc_seqnum;

  /* Last received Database Description packet */
  struct ospf6_dbdesc  dbdesc_last;

  /* LS-list */
  struct ospf6_lsdb *summary_list;
  struct ospf6_lsdb *request_list;
  struct ospf6_lsdb *retrans_list;

  /* LSA list for message transmission */
  struct ospf6_lsdb *dbdesc_list;
  struct ospf6_lsdb *lsreq_list;
  struct ospf6_lsdb *lsupdate_list;
  struct ospf6_lsdb *lsack_list;

};

/* Neighbor state */
#define OSPF6_NEIGHBOR_DOWN     1
#define OSPF6_NEIGHBOR_ATTEMPT  2
#define OSPF6_NEIGHBOR_INIT     3
#define OSPF6_NEIGHBOR_TWOWAY   4
#define OSPF6_NEIGHBOR_EXSTART  5
#define OSPF6_NEIGHBOR_EXCHANGE 6
#define OSPF6_NEIGHBOR_LOADING  7
#define OSPF6_NEIGHBOR_FULL     8

extern int hello_received(struct thread *);
extern int twoway_received(struct thread *);
extern int negotiation_done(struct thread *);
extern int adj_ok(struct thread *);
extern int oneway_received(struct thread *);

struct ospf6_neighbor * ospf6_neighbor_lookup(u_int32_t router_id, struct ospf6_interface *oi);

struct ospf6_neighbor * ospf6_neighbor_create(u_int32_t router_id, struct ospf6_interface * oi);

#endif
