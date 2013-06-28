#include <arpa/inet.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#include "routeflow-common.h"
#include "paxosflow-common.h"
#include "dblist.h"
#include "rfpbuf.h"
#include "rfp-msgs.h"

static const struct raw_info *raw_info_get(enum rfpraw);
static struct raw_instance *raw_instance_get(const struct raw_info *,
                                             uint8_t version);

/* Returns a transaction ID to use for an outgoing OpenFlow message. */
static uint32_t
alloc_xid(void)
{
    static uint32_t next_xid = 1;
    return htonl(next_xid++);
}

/* Updates the 'length' field of the OpenFlow message in 'buf' to
 * 'buf->size'. */
void
rfpmsg_update_length(struct rfpbuf *buf)
{
    struct rfp_header *rh = (struct rfp_header *)rfpbuf_at_assert(buf, 0, sizeof *rh);
    rh->length = htons(buf->size);
}

/* Allocates and returns a new ofpbuf that contains an OpenFlow header for
 * 'raw' with OpenFlow version 'version' and a fresh OpenFlow transaction ID.
 * The ofpbuf has enough tailroom for the minimum body length of 'raw', plus
 * 'extra_tailroom' additional bytes.
 *
 * Each 'raw' value is valid only for certain OpenFlow versions.  The caller
 * must specify a valid (raw, version) pair.
 *
 * In the returned ofpbuf, 'l2' points to the beginning of the OpenFlow header
 * and 'l3' points just after it, to where the message's body will start.  The
 * caller must actually allocate the body into the space reserved for it,
 * e.g. with ofpbuf_put_uninit().
 *
 * The caller owns the returned ofpbuf and must free it when it is no longer
 * needed, e.g. with ofpbuf_delete(). */
struct rfpbuf *
routeflow_alloc(uint8_t type, uint8_t version, size_t routeflow_len)
{
    return routeflow_alloc_xid(type, version, alloc_xid(), routeflow_len);
}

static void routeflow_put__(uint8_t type, uint8_t version, uint32_t xid,
                         size_t routeflow_len, struct rfpbuf *);

/* Same as ofpraw_alloc() but the caller provides the transaction ID. */
struct rfpbuf *
routeflow_alloc_xid(uint8_t type, uint8_t version, uint32_t xid,
                 size_t routeflow_len)
{
    struct rfpbuf *buf = rfpbuf_new(routeflow_len);
    routeflow_put__(type, version, xid, routeflow_len, buf);
    return buf;
}

static void routeflow_put__(uint8_t type, uint8_t version, uint32_t xid,
                         size_t routeflow_len, struct rfpbuf *buf)
{
  struct rfp_header *rh;

  assert(routeflow_len >= sizeof *rh);
  assert(routeflow_len <= UINT16_MAX);

  buf->l2 = rfpbuf_put_uninit(buf, routeflow_len);

  rh = buf->l2;
  rh->version = version;
  rh->type = type;
  rh->length = htons(routeflow_len);
  rh->xid = xid;
  memset(rh + 1, 0, routeflow_len - sizeof *rh);
}

/* Allocates and returns a new ofpbuf that contains an OpenFlow header for
 * 'raw' with OpenFlow version 'version' and a fresh OpenFlow transaction ID.
 * The ofpbuf has enough tailroom for the minimum body length of 'raw', plus
 * 'extra_tailroom' additional bytes.
 *
 * Each 'raw' value is valid only for certain OpenFlow versions.  The caller
 * must specify a valid (raw, version) pair.
 *
 * In the returned ofpbuf, 'l2' points to the beginning of the OpenFlow header
 * and 'l3' points just after it, to where the message's body will start.  The
 * caller must actually allocate the body into the space reserved for it,
 * e.g. with ofpbuf_put_uninit().
 *
 * The caller owns the returned ofpbuf and must free it when it is no longer
 * needed, e.g. with ofpbuf_delete(). */
struct rfpbuf *
paxosflow_alloc(uint8_t type, uint8_t version, size_t paxosflow_len, uint32_t src, uint32_t dst)
{
    return paxosflow_alloc_xid(type, version, alloc_xid(), paxosflow_len, src, dst);
}

static void paxosflow_put__(uint8_t type, uint8_t version, uint32_t src, uint32_t dst, uint32_t xid,
                         size_t paxosflow_len, struct rfpbuf *);

/* Same as ofpraw_alloc() but the caller provides the transaction ID. */
struct rfpbuf *
paxosflow_alloc_xid(uint8_t type, uint8_t version, uint32_t xid,
                 size_t paxosflow_len, uint32_t src, uint32_t dst)
{
    struct rfpbuf *buf = rfpbuf_new(paxosflow_len);
    paxosflow_put__(type, version, src, dst, xid, paxosflow_len, buf);
    return buf;
}

static void paxosflow_put__(uint8_t type, uint8_t version, uint32_t src, uint32_t dst, uint32_t xid,
                         size_t paxosflow_len, struct rfpbuf *buf)
{
  struct pfp_header *rh;

  assert(paxosflow_len >= sizeof *rh);
  assert(paxosflow_len <= UINT16_MAX);

  buf->l2 = rfpbuf_put_uninit(buf, paxosflow_len);

  rh = buf->l2;
  rh->version = version;
  rh->src = src;
  rh->dst = dst;
  rh->type = type;
  rh->length = htons(paxosflow_len);
  rh->xid = xid;
  memset(rh + 1, 0, paxosflow_len - sizeof *rh);
}
