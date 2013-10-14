#ifndef OSPF6_LSA_H
#define OSPF6_LSA_H

/* Masks for LS Type : RFC 2740 A.4.2.1 "LS type" */
#define OSPF6_LSTYPE_UBIT_MASK        0x8000
#define OSPF6_LSTYPE_SCOPE_MASK       0x6000
#define OSPF6_LSTYPE_FCODE_MASK       0x1fff

/* LSA scope */
#define OSPF6_SCOPE_LINKLOCAL  0x0000
#define OSPF6_SCOPE_AREA       0x2000
#define OSPF6_SCOPE_AS         0x4000
#define OSPF6_SCOPE_RESERVED   0x6000

/* XXX U-bit handling should be treated here */
#define OSPF6_LSA_SCOPE(type) \
    (ntohs (type) & OSPF6_LSTYPE_SCOPE_MASK)

/* LSA Header */
struct ospf6_lsa_header
{
  u_int16_t age;        /* LS age */
  u_int16_t type;       /* LS type */
  u_int32_t id;         /* Link State ID */
  u_int32_t adv_router; /* Advertising Router */
  u_int32_t seqnum;     /* LS sequence number */
  u_int16_t checksum;   /* LS checksum */
  u_int16_t length;     /* LSA length */
};

#define OSPF6_LSA_HEADER_END(h) \
    ((caddr_t)(h) + sizeof (struct ospf6_lsa_header))
#define OSPF6_LSA_SIZE(h) \
    (ntohs (((struct ospf6_lsa_header *) (h))->length))
#define OSPF6_LSA_IS_SAME(L1, L2) \
    ((L1)->header->adv_router == (L2)->header->adv_router && \
        (L1)->header->id == (L2)->header->id && \
        (L1)->header->type == (L2)->header->type)
#define OSPF6_LSA_IS_MAXAGE(L) (ospf6_lsa_age_current (L) == MAXAGE)
#define OSPF6_LSA_IS_CHANGED(L1, L2) ospf6_lsa_is_changed (L1, L2)

struct ospf6_lsa
{
  char name[64]; /* dump string */

  struct list node;

  unsigned char lock; /* reference counter */
  unsigned char flag; /* special meaning (e.g. floodback) */

  struct timeval birth;          /* tv_sec when LS age 0 */
  struct timeval originated;     /* used by MinLSInterval check */
  struct timeval received;       /* used by MinLSArrival check */
  struct timeval installed;

  struct thread  *expire;
  struct thread  *refresh;        /* For self-originated LSA */

  int retrans_count;

  struct ospf6_lsdb *lsdb;

  /* lsa instance */
  struct ospf6_lsa_header *header;
};

#define OSPF6_LSA_HEADERONLY 0x01

extern u_int16_t ospf6_lsa_age_current (struct ospf6_lsa *);
extern int ospf6_lsa_compare (struct ospf6_lsa *, struct ospf6_lsa *);
extern struct ospf6_lsa * ospf6_lsa_copy (struct ospf6_lsa *lsa);
extern struct ospf6_lsa * ospf6_lsa_create_headeronly (struct ospf6_lsa_header *header);

#endif