#include <arpa/inet.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#include "routeflow-common.h"
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

/*
static void
rfpraw_put__(enum rfpraw raw, uint8_t version, uint32_t xid,
             size_t extra_tailroom, struct rfpbuf *buf)
{
    const struct raw_info *info = raw_info_get(raw);
    const struct raw_instance *instance = raw_instance_get(info, version);
    const struct rfphdrs *hdrs = &instance->hdrs;
    struct rfp_header *rh;

    rfpbuf_prealloc_tailroom(buf, (instance->hdrs_len + info->min_body
                                   + extra_tailroom));
    buf->l2 = rfpbuf_put_uninit(buf, instance->hdrs_len);
    buf->l3 = rfpbuf_tail(buf);

    oh = buf->l2;
    oh->version = version;
    oh->type = hdrs->type;
    oh->length = htons(buf->size);
    oh->xid = xid;

    if (hdrs->type == OFPT_VENDOR) {
        struct nicira_header *nh = buf->l2;

        assert(hdrs->vendor == NX_VENDOR_ID);
        nh->vendor = htonl(hdrs->vendor);
        nh->subtype = htonl(hdrs->subtype);
    } else if (version == OFP10_VERSION
               && (hdrs->type == OFPT10_STATS_REQUEST ||
                   hdrs->type == OFPT10_STATS_REPLY)) {
        struct ofp10_stats_msg *osm = buf->l2;

        osm->type = htons(hdrs->stat);
        osm->flags = htons(0);
        if (hdrs->stat == OFPST_VENDOR) {
            struct ofp10_vendor_stats_msg *ovsm = buf->l2;

            ovsm->vendor = htonl(hdrs->vendor);
            if (hdrs->vendor == NX_VENDOR_ID) {
                struct nicira10_stats_msg *nsm = buf->l2;

                nsm->subtype = htonl(hdrs->subtype);
                memset(nsm->pad, 0, sizeof nsm->pad);
            } else {
                NOT_REACHED();
            }
        }
    } else if (version != OFP10_VERSION
               && (hdrs->type == OFPT11_STATS_REQUEST ||
                   hdrs->type == OFPT11_STATS_REPLY)) {
        struct ofp11_stats_msg *osm = buf->l2;

        osm->type = htons(hdrs->stat);
        osm->flags = htons(0);
        memset(osm->pad, 0, sizeof osm->pad);

        if (hdrs->stat == OFPST_VENDOR) {
            struct ofp11_vendor_stats_msg *ovsm = buf->l2;

            ovsm->vendor = htonl(hdrs->vendor);
            if (hdrs->vendor == NX_VENDOR_ID) {
                struct nicira11_stats_msg *nsm = buf->l2;

                nsm->subtype = htonl(hdrs->subtype);
            } else {
                NOT_REACHED();
            }
        }
    }
} */

static void rfpmsgs_init(void);
/*
static const struct raw_info *
raw_info_get(enum rfpraw raw)
{
    rfpmsgs_init();

    assert(raw < ARRAY_SIZE(raw_infos));
    return &raw_infos[raw];
}

static struct raw_instance *
raw_instance_get(const struct raw_info *info, uint8_t version)
{
    assert(version >= info->min_version && version <= info->max_version);
    return &info->instances[version - info->min_version];
}

static void
rfpmsgs_init(void)
{
    const struct raw_info *info;

    if (raw_instance_map.buckets) {
        return;
    }

    hmap_init(&raw_instance_map);
    for (info = raw_infos; info < &raw_infos[ARRAY_SIZE(raw_infos)]; info++)
    {
        int n_instances = info->max_version - info->min_version + 1;
        struct raw_instance *inst;

        for (inst = info->instances;
             inst < &info->instances[n_instances];
             inst++) {
            inst->hdrs_len = ofphdrs_len(&inst->hdrs);
            hmap_insert(&raw_instance_map, &inst->hmap_node,
                        ofphdrs_hash(&inst->hdrs));
        }
    }
}
*/
