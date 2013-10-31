#ifndef OSPF6_INTRA_H
#define OSPF6_INTRA_H

/* Link-LSA */
struct ospf6_link_lsa
{
  u_char priority;
  u_char options[3];
  struct in6_addr linklocal_addr;
  u_int32_t prefix_num;
  /* followed by ospf6 prefix(es) */
};

#define OSPF6_LINK_LSA_SCHEDULE(oi) \
  do { \
    if (! (oi)->thread_link_lsa) \
      (oi)->thread_link_lsa = \
        thread_add_event (master, ospf6_link_lsa_originate, oi, 0); \
  } while (0)

extern int ospf6_link_lsa_originate(struct thread *);

#endif
