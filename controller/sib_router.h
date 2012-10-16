#ifndef SIB_ROUTER_H
#define SIB_ROUTER_H

enum sib_router_state {
    S_CONNECTING,               /* Waiting for connection to complete. */
    S_FEATURES_REPLY,           /* Waiting for features reply. */
    S_ROUTING,                  /* Switching flows. */
};

struct sib_router {
    struct rconn *rconn;
    enum sib_router_state state;
};


struct sib_router * sib_router_create(struct rconn *, struct router ** routers, int n_routers);
void sib_router_run(struct sib_router *);
void sib_router_wait(struct sib_router *);
bool sib_router_is_alive(const struct sib_router *);
void sib_router_wait(struct sib_router *);
void sib_router_destroy(struct sib_router *);

#endif
