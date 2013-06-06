#ifndef BGP_CTRL_CLIENT
#define BGP_CTRL_CLIENT

struct bgp_ctrl_client
{
  int sock;

  struct in6_addr * ctrl_addr;

  int fail;

  struct rfpbuf * ibuf;
  struct rfpbuf * obuf;

  struct thread * t_read;
  struct thread * t_write;
  struct thread * t_connect;
  struct thread * t_connected;

  int state;

  /* Pointer to the callback functions */
  int (*features_reply) (struct bgp_ctrl_client *, struct rfpbuf *);
  int (*routes_reply) (struct bgp_ctrl_client *, struct rfpbuf *);
};

struct bgp_ctrl_client * bgp_ctrl_client_new();
void bgp_ctrl_client_init(struct bgp_ctrl_client * bgp_ctrl_client, struct in6_addr * ctrl_addr);

#endif
