#ifndef CTRL_CLIENT_H
#define CTRL_CLIENT_H

struct ctrl_client
{
  int sock;

  struct in6_addr * ctrl_addr;
  struct in6_addr * sibling_addr;
  unsigned int current_xid;

  int fail;

  struct rfpbuf * ibuf;
  struct rfpbuf * obuf;

  struct thread * t_read; 
  struct thread * t_write;
  struct thread * t_connect;     /* thread to call when connecting */
  struct thread * t_connected;   /* thread to call when already connected */
 
  char * interface_name;
  int state; 

  /* Pointer to the callback functions */
  int (*features_reply) (struct ctrl_client *, struct rfpbuf *);
  int (*routes_reply) (struct ctrl_client *, struct rfpbuf *);
  int (*address_add_v4)(struct ctrl_client *, struct rfpbuf *);
  int (*address_add_v6)(struct ctrl_client *, struct rfpbuf *);
  int (*leader_elect) ();
};

extern struct ctrl_client * ctrl_client_new();
extern void ctrl_client_init(struct ctrl_client * ctrl_client, 
                             struct in6_addr * ctrl_addr, 
                             struct in6_addr * sibling_addr);
extern void ctrl_client_interface_init(struct ctrl_client *, char *);
int fwd_message_send(struct ctrl_client * ctrl_client);
int ctrl_client_start(struct ctrl_client * ctrl_client);
extern int ctrl_client_if_addr_req(struct ctrl_client * ctrl_client);
/* TCP socket connection to controller */
extern int ctrl_client_socket(struct in6_addr * ctrl_addr,
                              struct in6_addr * sibling_addr);

#endif
