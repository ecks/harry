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
    RFPT_FEATURES_REQUEST,    /* Controller/switch message */
    RFPT_FEATURES_REPLY,      /* Controller/switch message */
    RFPT_GET_CONFIG_REQUEST,  /* Controller/switch message */
    RFPT_GET_CONFIG_REPLY,    /* Controller/switch message */
    RFPT_SET_CONFIG           /* Controller/switch message */
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

/* Description of a physical port */
struct rfp_phy_port {
    uint16_t port_no;
//    uint8_t hw_addr[RFP_ETH_ALEN];
    char name[RFP_MAX_PORT_NAME_LEN]; /* Null-terminated */

//    uint32_t config;        /* Bitmap of OFPPC_* flags. */
//    uint32_t state;         /* Bitmap of OFPPS_* flags. */

    /* Bitmaps of OFPPF_* that describe features.  All bits zeroed if
     * unsupported or unavailable. */
//    uint32_t curr;          /* Current features. */
//    uint32_t advertised;    /* Features being advertised by the port. */
//    uint32_t supported;     /* Features supported by the port. */
//    uint32_t peer;          /* Features advertised by peer. */
};
//RFP_ASSERT(sizeof(struct rfp_phy_port) == 48);

/* Switch features. */
struct rfp_switch_features {
    struct rfp_header header;
    uint64_t datapath_id;   /* Datapath unique ID.  The lower 48-bits are for
                               a MAC address, while the upper 16-bits are
                               implementer-defined. */
    /* Port info.*/
    struct rfp_phy_port ports[0];  /* Port definitions.  The number of ports
                                      is inferred from the length field in
                                      the header. */
};

#endif
