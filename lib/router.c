#include "stdlib.h"
#include "stdint.h"
#include "stdio.h"
#include "stdbool.h"

#include "rconn.h"
#include "dblist.h"
#include "rfp-msgs.h"
#include "rfpbuf.h"
#include "routeflow-common.h"
#include "router.h"

enum router_state {
    S_CONNECTING,               /* Waiting for connection to complete. */
    S_FEATURES_REPLY,           /* Waiting for features reply. */
    S_ROUTING,                  /* Switching flows. */
};

struct router {
    struct rconn *rconn;
    enum router_state state;
};

static void send_features_request(struct router *);
static void router_process_packet(struct router *, const struct rfpbuf *);

/* Creates and returns a new learning switch whose configuration is given by
 * 'cfg'.
 *
 * 'rconn' is used to send out an OpenFlow features request. */
struct router *
router_create(struct rconn *rconn)
{
  struct router * rt;

  rt = malloc(sizeof *rt);
  rt->rconn = rconn;
  rt->state = S_CONNECTING;

  return rt;
}

static void
router_handshake(struct router * rt)
{
  send_features_request(rt);
}

void router_run(struct router * rt)
{
  int i;

  rconn_run(rt->rconn);

  if(rt->state == S_CONNECTING)
  {
//    if(rconn_get_version(rt->rconn) != -1)
//    {
      router_handshake(rt);
      rt->state = S_FEATURES_REPLY;
//    }
    return;
  }

  for(i = 0; i < 50; i++)
  {
    struct rfpbuf * msg;
    
    msg = rconn_recv(rt->rconn);
    if(!msg)
    {
      break;
    }

    router_process_packet(rt, msg);
    rfpbuf_delete(msg);
  }
}

void
router_wait(struct router * rt)
{
  rconn_run_wait(rt->rconn);
  rconn_recv_wait(rt->rconn);
}

static void
router_process_packet(struct router * rt, const struct rfpbuf * msg)
{
  enum rfptype type;
  struct rfp_header * rh;

  rh = msg->data;

  switch(rh->type)
  {
    case RFPT_FEATURES_REPLY:
      printf("features reply\n");
      break;

    default:
      printf("unknown packet\n");
  }
}

bool
router_is_alive(const struct router *rt)
{
    return rconn_is_alive(rt->rconn);
}

/* Destroys 'sw'. */
void
router_destroy(struct router *sw)
{
  if(sw)
  {
    rconn_destroy(sw->rconn);
    free(sw);
  } 
}

static void
send_features_request(struct router *rt)
{
  struct rfpbuf * buffer;
 
  buffer = routeflow_alloc(RFPT_FEATURES_REQUEST, RFP10_VERSION, sizeof(struct rfp_header));

  rconn_send(rt->rconn, buffer);
}
