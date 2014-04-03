#ifndef OSPF6_SIBLING_DEBUG
#define OSPF6_SIBLING_DEBUG

#define OSPF6_SIBLING_DEBUG_MSG 0x01

#define OSPF6_SIBLING_DEBUG_SISIS 0x01

#define OSPF6_SIBLING_DEBUG_REPLICA 0x01

#define OSPF6_SIBLING_DEBUG_RESTART 0x01

#define OSPF6_SIBLING_DEBUG_NEIGHBOR 0x01

#define OSPF6_SIBLING_DEBUG_CTRL_CLIENT 0x01

#define OSPF6_SIBLING_DEBUG_INTERFACE 0x01

#define OSPF6_SIBLING_DEBUG_FLOOD 0x01

#define OSPF6_SIBLING_DEBUG_AREA 0x01

#define OSPF6_SIBLING_DEBUG_SPF 0x01

#define IS_OSPF6_SIBLING_DEBUG_MSG (ospf6_sibling_debug_msg & OSPF6_SIBLING_DEBUG_MSG)

#define IS_OSPF6_SIBLING_DEBUG_SISIS (ospf6_sibling_debug_sisis & OSPF6_SIBLING_DEBUG_SISIS)

#define IS_OSPF6_SIBLING_DEBUG_REPLICA (ospf6_sibling_debug_replica & OSPF6_SIBLING_DEBUG_REPLICA)

#define IS_OSPF6_SIBLING_DEBUG_NEIGHBOR (ospf6_sibling_debug_neighbor & OSPF6_SIBLING_DEBUG_NEIGHBOR)

#define IS_OSPF6_SIBLING_DEBUG_CTRL_CLIENT (ospf6_sibling_debug_ctrl_client & OSPF6_SIBLING_DEBUG_CTRL_CLIENT)

#define IS_OSPF6_SIBLING_DEBUG_INTERFACE (ospf6_sibling_debug_interface & OSPF6_SIBLING_DEBUG_INTERFACE)

#define IS_OSPF6_SIBLING_DEBUG_FLOOD (ospf6_sibling_debug_flood & OSPF6_SIBLING_DEBUG_FLOOD)

#define IS_OSPF6_SIBLING_DEBUG_AREA (ospf6_sibling_debug_area & OSPF6_SIBLING_DEBUG_AREA)

#define IS_OSPF6_SIBLING_DEBUG_SPF (ospf6_sibling_debug_spf & OSPF6_SIBLING_DEBUG_SPF)

extern unsigned long ospf6_sibling_debug_msg;

extern unsigned long ospf6_sibling_debug_sisis;

extern unsigned long ospf6_sibling_debug_replica;

extern unsigned long ospf6_sibling_debug_restart;

extern unsigned long ospf6_sibling_debug_neighbor;

extern unsigned long ospf6_sibling_debug_ctrl_client;

extern unsigned long ospf6_sibling_debug_interface;

extern unsigned long ospf6_sibling_debug_flood;

extern unsigned long ospf6_sibling_debug_area;

extern unsigned long ospf6_sibling_debug_spf;

void ospf6_sibling_debug_init(void);

#endif
