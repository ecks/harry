#ifndef CTRL_CLIENT_H
#define CTRL_CLIENT_H

enum ctrl_client_state
{
  CTRL_CONNECTING,           /* not connected */
  CTRL_SENT_HELLO,           /* Sent HELLO */
  CTRL_RCVD_HELLO,           /* Received HELLO */
  CTRL_INTERFACE_UP,         /* Interface is up for the Appropriate Controller */
  CTRL_LEAD_ELECT_RCVD,    /* Leader Election Message Received */
  CTRL_CONNECTED             /* connectionn established */
};

struct ctrl_client
{
  int sock;

  struct in6_addr * ctrl_addr;
  struct in6_addr * sibling_addr;
  unsigned int current_xid;

  int fail;

  struct rfpbuf * ibuf;
  struct rfpbuf * obuf;

  struct list node;

  struct thread * t_read; 
  struct thread * t_write;
  struct thread * t_connect;     /* thread to call when connecting */
  struct thread * t_connected;   /* thread to call when already connected */
 
  char * interface_name;
  struct connected * inter_con;

  enum ctrl_client_state state; 

  struct list * if_list;

  unsigned int hostnum;

  /* Pointer to the callback functions */
  int (*features_reply) (struct ctrl_client *, struct rfpbuf *);
  int (*routes_reply) (struct ctrl_client *, struct rfpbuf *);
  int (*address_add_v4)(struct ctrl_client *, struct rfpbuf *);
  int (*address_add_v6)(struct ctrl_client *, struct rfpbuf *);
};

extern struct ctrl_client * ctrl_client_new();
extern void ctrl_client_init(struct ctrl_client * ctrl_client, 
                             struct in6_addr * ctrl_addr, 
                             struct in6_addr * sibling_addr);
extern void ctrl_client_interface_init(struct ctrl_client *, char *);
int fwd_message_send(struct ctrl_client * ctrl_client);
int ctrl_client_start(struct ctrl_client * ctrl_client);
extern int ctrl_client_route_set(struct ctrl_client *, struct ospf6_route *);
extern int ctrl_client_route_unset(struct ctrl_client *, struct ospf6_route *);
extern int ctrl_client_if_addr_req(struct ctrl_client * ctrl_client);
/* TCP socket connection to controller */
extern int ctrl_client_socket(struct in6_addr * ctrl_addr,
                              struct in6_addr * sibling_addr);

#endif
