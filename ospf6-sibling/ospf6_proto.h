#ifndef OSPF6_PROTO_H
#define OSPF6_PRTO_H

/* OSPF protocol version */
#define OSPFV3_VERSION           3

/* Architectural constants */
#define LS_REFRESH_TIME                1800  /* 30 min */
#define MIN_LS_ARRIVAL                    1
#define MAXAGE                         3600  /* 1 hour */
#define MAX_AGE_DIFF                    900  /* 15 min */
#define INITIAL_SEQUENCE_NUMBER  0x80000001  /* signed 32-bit integer */
#define MAX_SEQUENCE_NUMBER      0x7fffffff  /* signed 32-bit integer */

/* OSPF options */
/* present in HELLO, DD, LSA */
#define OSPF6_OPT_SET(x,opt)   ((x)[2] |=  (opt))
#define OSPF6_OPT_ISSET(x,opt) ((x)[2] &   (opt))
#define OSPF6_OPT_CLEAR(x,opt) ((x)[2] &= ~(opt))
#define OSPF6_OPT_CLEAR_ALL(x) ((x)[0] = (x)[1] = (x)[2] = 0)

#define OSPF6_OPT_DC (1 << 5)   /* Demand Circuit handling Capability */
#define OSPF6_OPT_R  (1 << 4)   /* Forwarding Capability (Any Protocol) */
#define OSPF6_OPT_N  (1 << 3)   /* Handling Type-7 LSA Capability */
#define OSPF6_OPT_MC (1 << 2)   /* Multicasting Capability */
#define OSPF6_OPT_E  (1 << 1)   /* AS External Capability */
#define OSPF6_OPT_V6 (1 << 0)   /* IPv6 forwarding Capability */

/* OSPF6 Prefix */
struct ospf6_prefix
{
  u_int8_t prefix_length;
  u_int8_t prefix_options;
  union {
    u_int16_t _prefix_metric;
    u_int16_t _prefix_referenced_lstype;
  } u;
#define prefix_metric        u._prefix_metric
#define prefix_refer_lstype  u._prefix_referenced_lstype
  /* followed by one address_prefix */
};

#define OSPF6_PREFIX_BODY(x) ((void *)(x) + sizeof (struct ospf6_prefix))

#define OSPF6_PREFIX_SPACE(x) ((((x) + 31) / 32) * 4)

#define OSPF6_PREFIX_SIZE(x) \
     (OSPF6_PREFIX_SPACE ((x)->prefix_length) + sizeof (struct ospf6_prefix))

#define OSPF6_PREFIX_NEXT(x) \
     ((struct ospf6_prefix *)((void *)(x) + OSPF6_PREFIX_SIZE (x)))
 
extern void ospf6_options_printbuf (u_char *options, char *buf, int size);

#endif
