#ifndef DATAPATH_H
#define DATAPATH_H

#define DP_MAX_PORTS 255

struct sw_port {
    char hw_name[RFP_MAX_PORT_NAME_LEN];
    struct list node;
    uint16_t port_no;
    uint32_t state;
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

int dp_new(struct datapath **, uint64_t dpid);
void add_controller(struct datapath *, const char *target);
struct sw_port dp_get_ports();
void dp_run(struct datapath *);

#endif
