#ifndef EXT_CLIENT_PROVIDER
#define EXT_CLIENT_PROVIDER

struct ext_client {
  struct ext_client_class * class;

  int sockfd;
  char * host;
  unsigned int ifindex;
  
  struct punter_ctrl * punter_ctrl;

  struct rfpbuf * ibuf;
  struct rfpbuf * obuf;
};

#define P(O) (O->ext_client) // access parent of class

void ext_client_init(char *, struct punter_ctrl *);
void ext_client_send(struct rfpbuf *, enum rfp_type);
void ext_client_recv(struct ext_client *);

struct ext_client_class {
  struct ext_client * (*init)(char *, struct punter_ctrl *);
  void (*send)(struct ext_client *);  
};

extern struct ext_client_class ospf6_ext_client_class;
extern struct ext_client_class bgp_ext_client_class;

#endif
