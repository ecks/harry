#include "stdio.h"
#include "stdlib.h"
#include "stdbool.h"
#include "stdint.h"
#include "stddef.h"
#include "errno.h"
#include "string.h"
#include "arpa/inet.h"

#include "lib/routeflow-common.h"
#include "lib/util.h"
#include "lib/byte-order.h"
#include "lib/dblist.h"
#include "lib/rfpbuf.h"
#include "lib/rfp-msgs.h"
#include "lib/rconn.h"
#include "lib/vconn.h"
#include "datapath.h"

/* The origin of a received OpenFlow message, to enable sending a reply. */
struct sender {
    struct rfconn *rfconn;      /* The device that sent the message. */
    uint32_t xid;               /* The OpenFlow transaction ID. */
};

/* A connection to a secure channel. */
struct rfconn {
    struct list node;
    struct datapath * dp;
    struct rconn *rconn;
};

void get_ports(struct list * ports);
static struct rfconn *rfconn_create(struct datapath *, struct rconn *);
static void rfconn_run(struct datapath *, struct rfconn *);
static void rfconn_destroy(struct rfconn *);

int fwd_control_input(struct datapath *, const struct sender *,
                      const void *, size_t);

int dp_new(struct datapath **dp_, uint64_t dpid)
{
    struct datapath *dp;

    dp = calloc(1, sizeof *dp);
    if (!dp) {
        return ENOMEM;
    }

    list_init(&dp->all_conns);

    get_ports(&dp->port_list);
 
    *dp_ = dp;
    return 0;
}

void
dp_run(struct datapath *dp)
{
  struct rfconn *rfconn, *next_rfconn;

  /* Talk ot remote controllers */
  LIST_FOR_EACH_SAFE(rfconn, next_rfconn, struct rfconn, node, &dp->all_conns)
  {
    rfconn_run(dp, rfconn);
  }
}

/* Creates a new controller for 'target' in 'mgr'.  update_controller() needs
 * to be called later to finish the new ofconn's configuration. */
void
add_controller(struct datapath *dp, const char *target)
{
  struct rfconn * rfconn;

  rfconn = rfconn_create(dp, rconn_create());
  rconn_connect(rfconn->rconn, target);
}

void
get_ports(struct list * ports)
{
  int retval;

  list_init(ports);
  if(api_add_all_ports(ports) != 0)
  {
    exit(1);
  }
}

static void
rfconn_run(struct datapath * dp, struct rfconn *r)
{
  int i;

  rconn_run(r->rconn);

  for(i = 0; i < 1; i++)
  {
    struct rfpbuf * buffer;
    struct rfp_header * rh;

    buffer = rconn_recv(r->rconn);
    if(!buffer)
    {
      break;
    }

    if (buffer->size >= sizeof *rh) 
    {
      struct sender sender;

      rh = (struct rfp_header *)buffer->data;
      sender.rfconn = r;
      sender.xid = rh->xid;
      fwd_control_input(dp, &sender, buffer->data, buffer->size);
    } 
    else 
    {
      printf("received too-short OpenFlow message\n");
    }
    rfpbuf_delete(buffer);
  }

  if(!rconn_is_alive(r->rconn))
  {
    rfconn_destroy(r);
  }
}

static void
rfconn_destroy(struct rfconn *r)
{
    if (r) 
    {
        list_remove(&r->node);
        rconn_destroy(r->rconn);
        free(r);
    }
}

static struct rfconn *
rfconn_create(struct datapath *dp, struct rconn *rconn)
{
  struct rfconn * rfconn;
  
  rfconn = malloc(sizeof *rfconn);
  rfconn->dp = dp;
  list_push_back(&dp->all_conns, &rfconn->node);
  rfconn->rconn = rconn;

  return rfconn;
}

static int
send_routeflow_buffer_to_remote(struct rfpbuf *buffer, struct rfconn *rfconn)
{
    int retval = rconn_send(rfconn->rconn, buffer);

    if (retval) {
        printf("send to %s failed: %s",
                     rconn_get_target(rfconn->rconn), strerror(retval));
    }
    return retval;
}

static int
send_routeflow_buffer(struct datapath *dp, struct rfpbuf *buffer,
                     const struct sender *sender)
{
    rfpmsg_update_length(buffer);
    if (sender) 
    {
        /* Send back to the sender. */
        return send_routeflow_buffer_to_remote(buffer, sender->rfconn);
    }
    else
    {
        return 0;
    }
}

struct rfpbuf *
make_openflow_reply(uint8_t type, uint8_t version, size_t openflow_len,
                    const struct sender *sender)
{
  struct rfpbuf * rfpbuf;
  rfpbuf = routeflow_alloc_xid(type, version, sender ? sender->xid : 0,
                             openflow_len);
  return rfpbuf;
}

static void
fill_port_desc(struct sw_port * p, struct rfp_phy_port * desc)
{
  desc->port_no = htons(p->port_no);
  strncpy((char *) desc->name, p->hw_name, sizeof desc->name);
}

static void
dp_send_features_reply(struct datapath *dp, const struct sender *sender)
{
    struct rfpbuf *buffer;
    struct rfp_switch_features *rfr;
    struct sw_port * p;

    buffer = make_openflow_reply(RFPT_FEATURES_REPLY, RFP10_VERSION,
                               sizeof(struct rfp_switch_features), sender);
    
    rfr = buffer->l2;
    rfr->datapath_id  = htonll(dp->id);
    LIST_FOR_EACH(p, struct sw_port, node, &dp->port_list)
    {
      
      struct rfp_phy_port * rpp = rfpbuf_put_uninit(buffer, sizeof(struct rfp_phy_port));
      memset(rpp, 0, sizeof *rpp);
      fill_port_desc(p, rpp);
    }
    send_routeflow_buffer(dp, buffer, sender);
}

static int
recv_features_request(struct datapath *dp, const struct sender *sender,
                      const void *msg)
{
    dp_send_features_reply(dp, sender);
    return 0;
}

/* 'msg', which is 'length' bytes long, was received from the control path.
 * Apply it to 'chain'. */
int
fwd_control_input(struct datapath *dp, const struct sender *sender,
                  const void *msg, size_t length)
{
    int (*handler)(struct datapath *, const struct sender *, const void *);
    struct rfp_header * rh;
    size_t min_size;

    /* Check encapsulated length. */
    rh = (struct rfp_header *) msg;
    if (ntohs(rh->length) > length) {
        return -EINVAL;
    }
//    assert(rh->version == OFP_VERSION);

    /* Figure out how to handle it. */
    switch (rh->type) 
    {
      case RFPT_FEATURES_REQUEST:
        printf("Receiving features request message\n");
        min_size = sizeof(struct rfp_header);
        handler = recv_features_request;
        break;
      default:
        return -EINVAL;
    }

        /* Handle it. */
    if (length < min_size)
        return -EFAULT;
    return handler(dp, sender, msg);
}
