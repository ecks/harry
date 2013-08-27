#ifndef RCONN_H
#define RCONN_H

#define STATES                                  \
    STATE(VOID, 1 << 0)                         \
    STATE(BACKOFF, 1 << 1)                      \
    STATE(CONNECTING, 1 << 2)                   \
    STATE(ACTIVE, 1 << 3)                       \
    STATE(IDLE, 1 << 4)
enum state {
#define STATE(NAME, VALUE) S_##NAME = VALUE,
    STATES
#undef STATE
};

/* A reliable connection to an OpenFlow switch or controller.
 *
 * See the large comment in rconn.h for more information. */
struct rconn {
    enum state state;

    struct vconn *vconn;
    char *target;               /* vconn name, passed to vconn_open(). */

//    struct list txq;            /* Contains "struct ofpbuf"s. */

//    int backoff;
//    int max_backoff;
//    time_t backoff_deadline;
//    time_t last_connected;
//    time_t last_disconnected;
//    unsigned int packets_sent;
//    unsigned int seqno;
//    int last_error;

    /* In S_ACTIVE and S_IDLE, probably_admitted reports whether we believe
     * that the peer has made a (positive) admission control decision on our
     * connection.  If we have not yet been (probably) admitted, then the
     * connection does not reset the timer used for deciding whether the switch
     * should go into fail-open mode.
     *
     * last_admitted reports the last time we believe such a positive admission
     * control decision was made. */
//    bool probably_admitted;
//    time_t last_admitted;

    /* These values are simply for statistics reporting, not used directly by
     * anything internal to the rconn (or ofproto for that matter). */
//    unsigned int packets_received;
//    unsigned int n_attempted_connections, n_successful_connections;
//    time_t creation_time;
//    unsigned long int total_time_connected;


    /* Throughout this file, "probe" is shorthand for "inactivity probe".  When
     * no activity has been observed from the peer for a while, we send out an
     * echo request as an inactivity probe packet.  We should receive back a
     * response.
     *
     * "Activity" is defined as either receiving an OpenFlow message from the
     * peer or successfully sending a message that had been in 'txq'. */
    int probe_interval;         /* Secs of inactivity before sending probe. */
//    time_t last_activity;       /* Last time we saw some activity. */

    /* When we create a vconn we obtain these values, to save them past the end
     * of the vconn's lifetime.  Otherwise, in-band control will only allow
     * traffic when a vconn is actually open, but it is nice to allow ARP to
     * complete even between connection attempts, and it is also polite to
     * allow traffic from other switches to go through to the controller
     * whether or not we are connected.
     *
     * We don't cache the local port, because that changes from one connection
     * attempt to the next. */
//    ovs_be32 local_ip, remote_ip;
//    ovs_be16 remote_port;
//    uint8_t dscp;

    /* Messages sent or received are copied to the monitor connections. */
//#define MAX_MONITORS 8
//    struct vconn *monitors[8];
//    size_t n_monitors;
};

struct vconn;

struct rconn * rconn_create();
void rconn_set_probe_interval(struct rconn *, int inactivity_probe_interval);
void rconn_connect(struct rconn *rc, const char *target);
void rconn_connect_unreliably(struct rconn *rc, struct vconn *vconn, const char *name);
bool rconn_exchanged_hellos(struct rconn * rc);
void rconn_disconnect(struct rconn *);
void rconn_destroy(struct rconn *);
void rconn_run(struct rconn *);
void rconn_run_wait(struct rconn *);
struct rfpbuf *rconn_recv(struct rconn *);
void rconn_recv_wait(struct rconn *, int (*func)(struct thread *), void * args);
int rconn_send(struct rconn *, struct rfpbuf *);
const char *rconn_get_target(const struct rconn *);

bool rconn_is_alive(const struct rconn *);
bool rconn_is_connected(const struct rconn *);
#endif
