#ifndef ROUTEFLOW_COMMON_H
#define ROUTEFLOW_COMMON_H

#ifdef SWIG
#define RFP_ASSERT(EXPR)        /* SWIG can't handle RFP_ASSERT. */
#elif !defined(__cplusplus)
/* Build-time assertion for use in a declaration context. */
#define RFP_ASSERT(EXPR)                                                \
        extern int (*build_assert(void))[ sizeof(struct {               \
                    unsigned int build_assert_failed : (EXPR) ? 1 : -1; })]
#else /* __cplusplus */
#include <boost/static_assert.hpp>
#define RFP_ASSERT BOOST_STATIC_ASSERT
#endif /* __cplusplus */

#define RFP_MAX_PORT_NAME_LEN  16

#define RFP_TCP_PORT  6633
#define RFP_SSL_PORT  6633

#define MAX_PORTS 255

#define RFP_ETH_ALEN 6          /* Bytes in an Ethernet address. */

/* Version number:
 *  * Non-experimental versions released: 0x01 0x02
 *   * Experimental versions released: 0x81 -- 0x99
 *    */
/* The most significant bit being set in the version field indicates an
 *  * experimental OpenFlow version.
 *   */
enum rfp_version {
    RFP10_VERSION = 0x01,
    RFP11_VERSION = 0x02,
    RFP12_VERSION = 0x03,
};

enum rfp_type {
    RFPT_HELLO,               /* Symmetric message */
    RFPT_ERROR,               /* Symmetric message */
    RFPT_ECHO_REQUEST,        /* Symmetric message */
    RFPT_ECHO_REPLY,          /* Symmetric message */
    RFPT_VENDOR,              /* Symmetric message */

    /* Switch configuration messages. */
    RFPT_FEATURES_REQUEST,        /* Controller/switch message */
    RFPT_FEATURES_REPLY,          /* Controller/switch message */
    RFPT_GET_CONFIG_REQUEST,	  /* Controller/switch message */
    RFPT_GET_CONFIG_REPLY,	  /* Controller/switch message */
    RFPT_SET_CONFIG,	          /* Controller/switch message */
    RFPT_STATS_ROUTES_REQUEST,    /* Controller/switch message */
    RFPT_STATS_ROUTES_REPLY,      /* Controller/switch message */
    RFPT_REDISTRIBUTE_REQUEST,    /* Sibling/controller message */
    RFPT_IPV4_ROUTE_ADD           /* Controller/sibling message */
};


/* Header on all OpenFlow packets. */
struct rfp_header {
    uint8_t version;    /* An OpenFlow version number, e.g. OFP10_VERSION. */
    uint8_t type;       /* One of the OFPT_ constants. */
    uint16_t length;    /* Length including this ofp_header. */
    uint32_t xid;       /* Transaction id associated with this packet.
                           Replies use the same id as was in the request
                           to facilitate pairing. */
};
RFP_ASSERT(sizeof(struct rfp_header) == 8);

/* OFPT_HELLO.  This message has an empty body, but implementations must
 * ignore any data included in the body, to allow for future extensions. */
struct rfp_hello {
    struct rfp_header header;
};

enum rfp_port_state
{
  RFPPS_LINK_DOWN = 1 << 0,
  
  RFPPS_FORWARD = 2 << 8
};

/* Description of a physical port */
struct rfp_phy_port {
    uint16_t port_no;
//    uint8_t hw_addr[RFP_ETH_ALEN];
    char name[RFP_MAX_PORT_NAME_LEN]; /* Null-terminated */

//    uint32_t config;        /* Bitmap of OFPPC_* flags. */
    uint32_t state;         /* Bitmap of RFPPS_* flags. */

    /* Bitmaps of OFPPF_* that describe features.  All bits zeroed if
     * unsupported or unavailable. */
//    uint32_t curr;          /* Current features. */
//    uint32_t advertised;    /* Features being advertised by the port. */
//    uint32_t supported;     /* Features supported by the port. */
//    uint32_t peer;          /* Features advertised by peer. */
};
//RFP_ASSERT(sizeof(struct rfp_phy_port) == 48);

/* Switch features. */
struct rfp_router_features {
    struct rfp_header header;
    uint64_t datapath_id;   /* Datapath unique ID.  The lower 48-bits are for
                               a MAC address, while the upper 16-bits are
                               implementer-defined. */
    /* Port info.*/
    struct rfp_phy_port ports[0];  /* Port definitions.  The number of ports
                                      is inferred from the length field in
                                      the header. */
};

struct rfp_nexthop {
    uint32_t nexthop;
    uint16_t ifindex;
};

struct rfp_route {
    uint16_t type;
    uint16_t flags;
    uint16_t prefixlen;
    uint32_t p;   // in_addr has size of 8*4 = 32
    uint32_t num_nexthops;
    struct rfp_nexthop nexthop[0];
    uint16_t distance;
    uint32_t metric;
    uint32_t table;
};

/* Routes Reply message */
struct rfp_stats_routes {
    struct rfp_header header;
    struct rfp_route routes[0];
};


struct rfp_ipv4_route {
  struct rfp_header header;
  uint16_t prefixlen;
  uint32_t p;
};

#endif
