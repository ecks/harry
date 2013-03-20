/* A connection to a secure channel. */
struct conn {
    struct list node;
    struct datapath * dp;
    struct rconn * rconn;
};

struct conn * conn_create(struct datapath *dp, struct rconn * rconn);
void conn_start(struct conn * conn, const char * target);
void conn_run(struct conn * conn);
void conn_wait(struct conn * conn);
void conn_forward_msg(struct conn * conn, struct rfpbuf * buf);


