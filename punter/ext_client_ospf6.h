#ifndef EXT_CLIENT_OSPF6
#define EXT_CLIENT_OSPF6

struct ext_client_ospf6
{
  struct punter_ctrl * punter_ctrl;

  int sockfd;
  char * host;
  unsigned int ifindex;
  struct in6_addr * linklocal;
  unsigned int mtu;

  struct thread * t_connect;
  struct thread * t_read;

  struct rfpbuf * ibuf;
  struct addrinfo * addr;

  int state;
};

extern struct ext_client_ospf6 * ext_client_ospf6_new();
extern void ext_client_ospf6_ospf6_init(struct ext_client_ospf6 *, char *, struct punter_ctrl *);
int ext_client_ospf6_send(struct rfpbuf * buf, struct ext_client_ospf6 * ext_client_ospf6);

#endif
