#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "stdbool.h"
#include "errno.h"
#include "string.h"
#include "assert.h"

#include "routeflow-common.h"
#include "socket-util.h"
#include "util.h"
#include "thread.h"
#include "rconn.h"
#include "vconn.h"

static void state_transition(struct rconn *, enum state);
static void rconn_set_target__(struct rconn *,
                               const char *target);
static int try_send(struct rconn *, struct rfpbuf *);
static void reconnect(struct rconn *);
static bool is_connected_state(enum state);

/* Creates and returns a new rconn.
 *
 * 'probe_interval' is a number of seconds.  If the interval passes once
 * without an OpenFlow message being received from the peer, the rconn sends
 * out an "echo request" message.  If the interval passes again without a
 * message being received, the rconn disconnects and re-connects to the peer.
 * Setting 'probe_interval' to 0 disables this behavior.
 *
 * 'max_backoff' is the maximum number of seconds between attempts to connect
 * to the peer.  The actual interval starts at 1 second and doubles on each
 * failure until it reaches 'max_backoff'.  If 0 is specified, the default of
 * 8 seconds is used.
 *
 * The new rconn is initially unconnected.  Use rconn_connect() or
 * rconn_connect_unreliably() to connect it. */
struct rconn *
rconn_create(int probe_interval)
{
    struct rconn *rc = malloc(sizeof *rc);

    rc->state = S_VOID;

    rc->vconn = NULL;
    rc->target = xstrdup("void");

    rconn_set_probe_interval(rc, probe_interval);

    return rc;
}

void
rconn_set_probe_interval(struct rconn *rc, int probe_interval)
{
    rc->probe_interval = probe_interval ? MAX(5, probe_interval) : 0;
}

int
rconn_get_probe_interval(const struct rconn *rc)
{
    return rc->probe_interval;
}

/* Drops any existing connection on 'rc', then sets up 'rc' to connect to
 * 'target' and reconnect as needed.  'target' should be a remote OpenFlow
 * target in a form acceptable to vconn_open().
 *
 * If 'name' is nonnull, then it is used in log messages in place of 'target'.
 * It should presumably give more information to a human reader than 'target',
 * but it need not be acceptable to vconn_open(). */
void
rconn_connect(struct rconn *rc, const char *target)
{
    rconn_disconnect(rc);
    rconn_set_target__(rc, target);
    reconnect(rc);
}

/* Drops any existing connection on 'rc', then configures 'rc' to use 
 * 'vconn'.  If the connection on 'vconn' drops, 'rc' will not reconnect on it
 * own. 
 * 
 * By default, the target obtained from vconn_get_name(vconn) is used in log
 * messages.  If 'name' is nonnull, then it is used instead.  It should 
 * presumably give more information to a human reader than the target, but it 
 * need not be acceptable to vconn_open(). */
void rconn_connect_unreliably(struct rconn *rc, struct vconn *vconn, const char *name)
{    
  assert(vconn != NULL);    
  rconn_disconnect(rc);
  rconn_set_target__(rc, vconn_get_name(vconn));    
//  rc->reliable = false;    
  rc->vconn = vconn;    
//  rc->last_connected = time_now();
  state_transition(rc, S_ACTIVE);
}

bool rconn_exchanged_hellos(struct rconn * rc)
{
  assert(rc->vconn != NULL);
  return vconn_is_connected(rc->vconn);
}

void
rconn_disconnect(struct rconn *rc)
{
    if (rc->state != S_VOID) {
        if (rc->vconn) {
            vconn_close(rc->vconn);
            rc->vconn = NULL;
        }
        rconn_set_target__(rc, "void");
        state_transition(rc, S_VOID);
    }
}

/* Disconnects 'rc' and frees the underlying storage. */
void
rconn_destroy(struct rconn *rc)
{
    if (rc) 
    {
        free(rc->target);
        vconn_close(rc->vconn);
        free(rc);
    }
}

static void
reconnect(struct rconn *rc)
{
    int retval;

    retval = vconn_open(rc->target, RFP10_VERSION, &rc->vconn, DSCP_DEFAULT);
    if (!retval) {
//        rc->remote_ip = vconn_get_remote_ip(rc->vconn);
//        rc->local_ip = vconn_get_local_ip(rc->vconn);
//        rc->remote_port = vconn_get_remote_port(rc->vconn);
        state_transition(rc, S_CONNECTING);
    } 
    else 
    {
        printf("%s: connection failed (%s)", rc->target, strerror(retval));
        rconn_disconnect(rc);
    }
}

static void
run_VOID(struct rconn *rc)
{
    /* Nothing to do. */
}

static void
run_BACKOFF(struct rconn *rc)
{

}

static void
run_CONNECTING(struct rconn *rc)
{
    int retval = vconn_connect(rc->vconn);
    if (!retval) {
        printf("%s: connected", rc->target);
        state_transition(rc, S_ACTIVE);
    } else if (retval != EAGAIN) {
            printf("%s: connection failed (%s)",
                      rc->target, strerror(retval));
        rconn_disconnect(rc);
    }
}

static void
run_ACTIVE(struct rconn *rc)
{

}

static void
run_IDLE(struct rconn *rc)
{

}

/* Performs whatever activities are necessary to maintain 'rc': if 'rc' is
 * disconnected, attempts to (re)connect, backing off as necessary; if 'rc' is
 * connected, attempts to send packets in the send queue, if any. */
void
rconn_run(struct rconn *rc)
{
    int old_state;
    size_t i;

    if (rc->vconn) {
        vconn_run(rc->vconn);
    }

    do {
        old_state = rc->state;
        switch (rc->state) {
#define STATE(NAME, VALUE) case S_##NAME: run_##NAME(rc); break;
            STATES
#undef STATE
        default:
            NOT_REACHED();
        }
    } while (rc->state != old_state);
}

/* Sends 'b' on 'rc'.  Returns 0 if successful, or ENOTCONN if 'rc' is not
 * currently connected.  Takes ownership of 'b'.
 *
 * If 'counter' is non-null, then 'counter' will be incremented while the
 * packet is in flight, then decremented when it has been sent (or discarded
 * due to disconnection).  Because 'b' may be sent (or discarded) before this
 * function returns, the caller may not be able to observe any change in
 * 'counter'.
 *
 * There is no rconn_send_wait() function: an rconn has a send queue that it
 * takes care of sending if you call rconn_run(), which will have the side
 * effect of waking up poll_block(). */
int
rconn_send(struct rconn *rc, struct rfpbuf *b)
{
    if (rconn_is_connected(rc)) {
            try_send(rc, b);
        return 0;
    } else {
        return ENOTCONN;
    }
}

/* Causes the next call to poll_block() to wake up when rconn_run() should be 
 * called on 'rc'. */
void
rconn_run_wait(struct rconn *rc)
{
  if(rc->vconn)
  {
    vconn_run_wait(rc->vconn);
  }
}

/* Attempts to receive a packet from 'rc'.  If successful, returns the packet;
 * otherwise, returns a null pointer.  The caller is responsible for freeing
 * the packet (with ofpbuf_delete()). */
struct rfpbuf *
rconn_recv(struct rconn *rc)
{
  if (rc->state & (S_ACTIVE | S_IDLE)) {
      struct rfpbuf *buffer;
      int error = vconn_recv(rc->vconn, &buffer);
      if (!error) {
          if (rc->state == S_IDLE) {
              state_transition(rc, S_ACTIVE);
          }
          return buffer;
     } 
     else if (error != EAGAIN) 
     {
       if(error == ECONNRESET)
       {
         printf("The connection has been closed\n");
         rconn_disconnect(rc);
         return NULL;
       }
       else
       {
         printf("An error has occured\n");
         rconn_disconnect(rc);
       }
     }
  }
  return NULL;
}

/* Causes the next call to poll_block() to wake up when a packet may be ready
 * to be received by vconn_recv() on 'rc'.  */
void
rconn_recv_wait(struct rconn *rc, int (*func)(struct thread *), void * args)
{
    if (rc->vconn) {
        vconn_wait(rc->vconn, WAIT_RECV, func, args);
    }
}

/* Returns 'rc''s target.  This is intended to be a string that may be passed
 * directly to, e.g., vconn_open(). */
const char *
rconn_get_target(const struct rconn *rc)
{
    return rc->target;
}

/* Returns true if 'rconn' is connected or in the process of reconnecting,
 * false if 'rconn' is disconnected and will not reconnect on its own. */
bool
rconn_is_alive(const struct rconn *rconn)
{
    return rconn->state != S_VOID;
}

/* Returns true if 'rconn' is connected, false otherwise. */
bool
rconn_is_connected(const struct rconn *rconn)
{
    return is_connected_state(rconn->state);
}

/* Set rc->target and rc->name to 'target' and 'name', respectively.  If 'name'
 * is null, 'target' is used.
 *
 * Also, clear out the cached IP address and port information, since changing
 * the target also likely changes these values. */
static void
rconn_set_target__(struct rconn *rc, const char *target)
{
    free(rc->target);
    rc->target = xstrdup(target);
//    rc->local_ip = 0;
//    rc->remote_ip = 0;
//    rc->remote_port = 0;
}
 
/* Tries to send a packet from 'rc''s send buffer.  Returns 0 if successful,
 * otherwise a positive errno value. */
static int
try_send(struct rconn *rc, struct rfpbuf * msg)
{
  int retval;

  retval = vconn_send(rc->vconn, msg);
  if(retval)
  {
    if(retval != EAGAIN)
    {
      rconn_disconnect(rc);
    }
    return retval;
  }

  return 0;
}

static void
state_transition(struct rconn *rc, enum state state)
{
//    VLOG_DBG("%s: entering %s", rc->name, state_name(state));
    rc->state = state;
//    rc->state_entered = time_now();
}

static bool
is_connected_state(enum state state)
{
    return (state & (S_ACTIVE | S_IDLE)) != 0;
}
