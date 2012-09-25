#ifndef RCONN_H
#define RCONN_H

struct vconn;

struct rconn * rconn_create();
void rconn_set_probe_interval(struct rconn *, int inactivity_probe_interval);
void rconn_connect(struct rconn *rc, const char *target);
void rconn_connect_unreliably(struct rconn *rc, struct vconn *vconn, const char *name);
void rconn_disconnect(struct rconn *);
void rconn_destroy(struct rconn *);
void rconn_run(struct rconn *);
void rconn_run_wait(struct rconn *);
struct rfpbuf *rconn_recv(struct rconn *);
void rconn_recv_wait(struct rconn *);
int rconn_send(struct rconn *, struct rfpbuf *);
const char *rconn_get_target(const struct rconn *);

bool rconn_is_alive(const struct rconn *);
bool rconn_is_connected(const struct rconn *);
#endif
