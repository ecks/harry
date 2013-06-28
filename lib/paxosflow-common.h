#ifndef PAXOSFLOW_COMMON_H
#define PAXOSFLOW_COMMON_H

/* Version number:
 *  * Non-experimental versions released: 0x01 0x02
 *   * Experimental versions released: 0x81 -- 0x99
 *    */
/* The most significant bit being set in the version field indicates an
 *  * experimental OpenFlow version.
 *   */
enum pfp_version {
    PFP10_VERSION = 0x01,
    PFP11_VERSION = 0x02,
    PFP12_VERSION = 0x03,
};

enum pfp_type {
    PFPT_HELLO,               /* Symmetric message */
    PFPT_ERROR,               /* Symmetric message */
    PFPT_ECHO_REQUEST,        /* Symmetric message */
    PFPT_ECHO_REPLY,          /* Symmetric message */
    PFPT_VENDOR,              /* Symmetric message */

    /* Switch configuration messages. */
    PFPT_FEATURES_REQUEST,        /* Controller/switch message */
    PFPT_FEATURES_REPLY,          /* Controller/switch message */
    PFPT_GET_CONFIG_REQUEST,	  /* Controller/switch message */
    PFPT_GET_CONFIG_REPLY,	  /* Controller/switch message */
    PFPT_SET_CONFIG,	          /* Controller/switch message */
    PFPT_STATS_ROUTES_REQUEST,    /* Controller/switch message */
    PFPT_STATS_ROUTES_REPLY,      /* Controller/switch message */
    PFPT_REDISTRIBUTE_REQUEST,    /* Sibling/controller message */
    PFPT_IPV4_ROUTE_ADD,          /* Controller/sibling message */

    /* Data forwarding messages. */
    PFPT_FORWARD_OSPF6,
    PFPT_FORWARD_BGP,
};


/* Header on all PaxosFlow packets. */
struct pfp_header {
    uint8_t version;    /* An OpenFlow version number, e.g. OFP10_VERSION. */
    uint8_t type;       /* One of the OFPT_ constants. */
    uint32_t src;       /* The source that sent the message */
    uint32_t dst;       /* The destination that is supposed to process the message */
    uint16_t length;    /* Length including this ofp_header. */
    uint32_t xid;       /* Transaction id associated with this packet.
                           Replies use the same id as was in the request
                           to facilitate pairing. */
};

/* PFPT_HELLO.  This message has an empty body, but implementations must
 * ignore any data included in the body, to allow for future extensions. */
struct pfp_hello {
    struct pfp_header header;
};

#endif
