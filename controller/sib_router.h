#ifndef SIB_ROUTER_H
#define SIB_ROUTER_H

enum sib_router_state {
    SIB_CONNECTING,               /* Waiting for connection to complete. */
    SIB_FEATURES_REPLY,           /* Waiting for features reply. */
    SIB_ROUTING,                  /* Switching flows. */
    SIB_DISCONNECTED,             /* Disconnected. */
};

struct sib_router {
    struct rconn *rconn;
    enum sib_router_state state;
    unsigned int current_ingress_xid;
    unsigned int current_egress_xid;
    struct list msgs_rcvd_queue;
};

struct sib_router * sib_router_create(struct rconn *);
int sib_router_run(struct thread *);
void sib_router_process_packet(struct sib_router *, struct rfpbuf *);
void sib_router_forward_ospf6(struct rfpbuf * msg, unsigned int current_ingress_xid);
void sib_router_forward_bgp(struct rfpbuf * msg);
void sib_router_wait(struct sib_router *);
bool sib_router_is_alive(const struct sib_router *);
void sib_router_wait(struct sib_router *);
void sib_router_destroy(struct sib_router *);

#endif
