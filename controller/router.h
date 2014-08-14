#ifndef ROUTER_H
#define ROUTER_H

enum router_state {
    R_CONNECTING,               /* Waiting for connection to complete. */
    R_CONNECTED,                /* The exchange of hello messages has been initiated. */
    R_FEATURES_REPLY,           /* Received features reply. */
    R_STATS_ROUTES_REPLY,       /* Received stats reply. */
    R_FEAT_ADDR_REPLY,          /* Received features and addr reply */
    R_FEAT_STATS_REPLY,         /* Received features and stats reply */ 
    R_ROUTING,                  /* Switching flows. */
};

enum port_state
{
  P_DISABLED = 1 << 0, 
  P_FORWARDING = 1 << 3
};

struct router {
  struct rconn *rconn;
  enum router_state state;
  struct list port_list;
  struct list * iflist;
};

struct iflist_
{
  struct list node;
  struct interface * ifp;
};


struct router * router_create(struct rconn *);
void router_forward(struct router * r, struct rfpbuf * msg);
int router_run(struct thread *);
void router_wait(struct router *);
bool router_is_alive(const struct router *);
void router_wait(struct router *);
void router_destroy(struct router *);

#endif
