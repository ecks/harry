#ifndef RFP_MSGS_H
#define RFP_MSGS_H

enum rfpraw {
/* Standard messages. */

    /* OFPT 1.0+ (0): uint8_t[]. */
    RFPRAW_RFPT_HELLO,

};

void rfpmsg_update_length(struct rfpbuf *);

/* Encoding messages using OFPRAW_* values. */
struct rfpbuf *routeflow_alloc(uint8_t type, uint8_t version,
                            size_t routeflow_len);
struct rfpbuf *routeflow_alloc_xid(uint8_t rfpraw, uint8_t version,
                                uint32_t xid, size_t routeflow_len);

/* Semantic identifiers for OpenFlow messages.
 *
 * Each OFPTYPE_* enumeration constant represents one or more concrete format
 * of OpenFlow message.  When two variants of a message have essentially the
 * same meaning, they are assigned a single OFPTYPE_* value.
 *
 * The comments here must follow a stylized form because the "extract-ofp-msgs"
 * program parses them at build time to generate data tables.  The format is
 * simply to list each OFPRAW_* enumeration constant for a given OFPTYPE_*,
 * each followed by a period. */
enum rfptype {
    /* Immutable messages. */
    RFPTYPE_HELLO,               /* OFPRAW_OFPT_HELLO. */
    RFPTYPE_ERROR,               /* OFPRAW_OFPT_ERROR. */
    RFPTYPE_ECHO_REQUEST,        /* OFPRAW_OFPT_ECHO_REQUEST. */
    RFPTYPE_ECHO_REPLY,          /* OFPRAW_OFPT_ECHO_REPLY. */
};

void rfpmsg_update_length(struct rfpbuf *);

#endif
