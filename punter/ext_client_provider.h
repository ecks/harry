#ifndef EXT_CLIENT_PROVIDER
#define EXT_CLIENT_PROVIDER

struct ext_client {
  struct ext_client_class * class;
  int sock;
  struct rfpbuf * ibuf;
  struct rfpbuf * obuf;
};

void ext_client_init(struct ext_client *, struct ext_client_class *);

struct ext_client_class {
  void (*init)(struct ext_client * ext_client);
  void (*send)(struct ext_client * ext_client);  
};

extern struct ext_client_class ospf6_ext_client_class;
extern struct ext_client_class bgp_ext_client_class;

#endif
