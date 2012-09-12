#ifndef RCONN_H
#define RCONN_H

struct vconn;

struct rconn *rconn_create();
void rconn_disconnect(struct rconn *);
void rconn_destroy(struct rconn *);
void rconn_run(struct rconn *);
struct rfpbuf *rconn_recv(struct rconn *);
int rconn_send(struct rconn *, struct rfpbuf *);
const char *rconn_get_target(const struct rconn *);

bool rconn_is_alive(const struct rconn *);
bool rconn_is_connected(const struct rconn *);
#endif
