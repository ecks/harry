#ifndef OSPF6_SIBLING_DEBUG
#define OSPF6_SIBLING_DEBUG

#define SET_FLAG(V, F) (V) |= (F)
#define UNSET_FLAG(V, F) (V) &= ~(F)

#define OSPF6_SIBLING_DEBUG_MSG 0x01

#define OSPF6_SIBLING_DEBUG_SISIS 0x01

#define OSPF6_SIBLING_DEBUG_REPLICA 0x01

#define IS_OSPF6_SIBLING_DEBUG_MSG (ospf6_sibling_debug_msg & OSPF6_SIBLING_DEBUG_MSG)

#define IS_OSPF6_SIBLING_DEBUG_SISIS (ospf6_sibling_debug_sisis & OSPF6_SIBLING_DEBUG_SISIS)

#define IS_OSPF6_SIBLING_DEBUG_REPLICA (ospf6_sibling_debug_replica & OSPF6_SIBLING_DEBUG_REPLICA)

extern unsigned long ospf6_sibling_debug_msg;

extern unsigned long ospf6_sibling_debug_sisis;

extern unsigned long ospf6_sibling_debug_replica;

void ospf6_sibling_debug_init(void);

#endif
