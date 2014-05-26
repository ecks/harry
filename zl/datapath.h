#ifndef DATAPATH_H
#define DATAPATH_H

#define DP_MAX_PORTS 255

struct sw_port {
    char hw_name[RFP_MAX_PORT_NAME_LEN];
    struct list node;
    uint16_t port_no;
    unsigned int mtu;
    uint32_t state;
    struct list connected;
};

struct sw_connected
{
  /* Attached interface */
  struct interface *ifp;

  /* Address of connected network. */
  struct prefix *address;

  struct list node;

  /* Peer or Broadcast address, depending on whether ZEBRA_IFA_PEER is set.
   *      Note: destination may be NULL if ZEBRA_IFA_PEER is not set. */
  struct prefix *destination;
};

struct datapath {
    /* Remote connections. */
    struct list all_conns;        /* All connections (including controller). */

    /* Unique identifier for this datapath */
    uint64_t  id;

    /* Router ports. */
    struct sw_port ports[DP_MAX_PORTS];
    struct list port_list;
    struct list ipv4_rib_routes;
#ifdef HAVE_IPV6
    struct list ipv6_rib_routes;
#endif
};

struct datapath * dp_new(uint64_t dpid);
void add_controller(struct datapath *, const char *target);
struct sw_port dp_get_ports();
void dp_run(struct datapath *);
void set_route_v6(struct route_ipv6 * route, struct in6_addr * nexthop_addr);

void dp_forward_zl_to_ctrl(struct datapath * dp, struct rfpbuf * buf);
void dp_forward_zl_to_punt(struct datapath * dp, struct rfpbuf * buf);

#endif
