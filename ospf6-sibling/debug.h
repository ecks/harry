#ifndef OSPF6_SIBLING_DEBUG
#define OSPF6_SIBLING_DEBUG

#define SET_FLAG(V, F) (V) |= (F)
#define UNSET_FLAG(V, F) (V) &= ~(F)

#define OSPF6_SIBLING_DEBUG_MSG 0x01

#define OSPF6_SIBLING_DEBUG_SISIS 0x01

#define OSPF6_SIBLING_DEBUG_REPLICA 0x01

#define OSPF6_SIBLING_DEBUG_NEIGHBOR 0x01

#define OSPF6_SIBLING_DEBUG_CTRL_CLIENT 0x01

#define OSPF6_SIBLING_DEBUG_INTERFACE 0x01

#define IS_OSPF6_SIBLING_DEBUG_MSG (ospf6_sibling_debug_msg & OSPF6_SIBLING_DEBUG_MSG)

#define IS_OSPF6_SIBLING_DEBUG_SISIS (ospf6_sibling_debug_sisis & OSPF6_SIBLING_DEBUG_SISIS)

#define IS_OSPF6_SIBLING_DEBUG_REPLICA (ospf6_sibling_debug_replica & OSPF6_SIBLING_DEBUG_REPLICA)

#define IS_OSPF6_SIBLING_DEBUG_NEIGHBOR (ospf6_sibling_debug_neighbor & OSPF6_SIBLING_DEBUG_NEIGHBOR)

#define IS_OSPF6_SIBLING_DEBUG_CTRL_CLIENT (ospf6_sibling_debug_ctrl_client & OSPF6_SIBLING_DEBUG_CTRL_CLIENT)

#define IS_OSPF6_SIBLING_DEBUG_INTERFACE (ospf6_sibling_debug_interface & OSPF6_SIBLING_DEBUG_INTERFACE)

extern unsigned long ospf6_sibling_debug_msg;

extern unsigned long ospf6_sibling_debug_sisis;

extern unsigned long ospf6_sibling_debug_replica;

extern unsigned long ospf6_sibling_debug_neighbor;

extern unsigned long ospf6_sibling_debug_ctrl_client;

extern unsigned long ospf6_sibling_debug_interface;

void ospf6_sibling_debug_init(void);

#endif
