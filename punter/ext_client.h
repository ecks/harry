#ifndef EXT_CLIENT
#define EXT_CLIENT

struct ext_client
{
  struct punter_ctrl * punter_ctrl;

  int sockfd;
  char * host;
  unsigned int ifindex;
  unsigned int mtu;

  struct thread * t_connect;
  struct thread * t_read;

  struct rfpbuf * ibuf;
  struct addrinfo * addr;

  int state;
};

extern struct ext_client * ext_client_new();
extern void ext_client_init(struct ext_client *, char *, struct punter_ctrl *);

#endif
