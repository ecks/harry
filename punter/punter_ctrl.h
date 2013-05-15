#ifndef PUNTER_CTRL
#define PUNTER_CTRL

struct sw_port {
    char hw_name[RFP_MAX_PORT_NAME_LEN];
    struct list node;
    uint16_t port_no;
    unsigned int mtu;
    uint32_t state;
};


struct punter_ctrl
{
  struct list * ipv4_addrs;
#ifdef HAVE_IPV6
  struct list * ipv6_addrs;
#endif
  struct list * port_list;
};

void punter_ctrl_init(char * host);

void punter_ext_to_zl_forward_msg(struct rfpbuf *);
void punter_zl_to_ext_forward_msg(enum rfp_type);

#endif
