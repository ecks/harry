#ifndef IF_H
#define IF_H

/* Interface structure */
struct interface
{
  char name[RFP_MAX_PORT_NAME_LEN + 1];

  unsigned int ifindex;
#define IFINDEX_INTERNAL        0

  /* Interface mtu */
  unsigned int mtu;

  u_char hw_addr[RFP_MAX_HWADDR_LEN];
  int hw_addr_len;

  /* pointer to daemon specific protocol */
  void * info;

  u_int32_t state;

  /* Connected address list. */
  struct list connected;

  struct list node;
};

/* Connected address structure. */
struct connected
{
  /* Attached interface */
  struct interface *ifp;

  /* Address of connected network. */
  struct prefix *address;

  struct list node;

  /* Peer or Broadcast address, depending on whether ZEBRA_IFA_PEER is set.
   *      Note: destination may be NULL if ZEBRA_IFA_PEER is not set. */
  struct prefix *destination;
};

void if_init();
struct interface *if_create (const char *name, int namelen);
struct interface *if_lookup_by_name (const char *ifname);
struct interface * if_lookup_by_index(const unsigned int index);
struct interface *if_get_by_name (const char *ifname);
int if_is_up(struct interface * ifp);
int if_is_broadcast(struct interface * ifp);
int if_is_pointopoint(struct interface * ifp);

/* Connected address functions. */
extern struct connected * connected_new(void);
extern void connected_free(struct connected *);
extern struct connected * connected_add_by_prefix(struct interface *,  struct prefix *, struct prefix *);

#endif
