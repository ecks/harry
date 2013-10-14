#ifndef OSPF6_PROTO_H
#define OSPF6_PRTO_H

/* OSPF protocol version */
#define OSPFV3_VERSION           3

/* Architectural constants */
#define MAXAGE                         3600  /* 1 hour */
#define MAX_AGE_DIFF                    900  /* 15 min */

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

#endif
