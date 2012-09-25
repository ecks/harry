#ifndef ROUTER_H
#define ROUTER_H

struct router * router_create(struct rconn *);
void router_run(struct router *);
void router_wait(struct router *);
bool router_is_alive(const struct router *);
void router_wait(struct router *);
void router_destroy(struct router *);

#endif
