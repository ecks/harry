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
  struct list *connected;

  struct list node;
};

void if_init();
struct interface *if_create (const char *name, int namelen);
struct interface *if_lookup_by_name (const char *ifname);
struct interface *if_get_by_name (const char *ifname);
int if_is_up(struct interface * ifp);
int if_is_pointtopoint(struct interface * ifp);

#endif
