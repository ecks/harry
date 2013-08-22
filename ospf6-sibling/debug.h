#ifndef OSPF6_SIBLING_DEBUG
#define OSPF6_SIBLING_DEBUG

#define SET_FLAG(V, F) (V) |= (F)
#define UNSET_FLAG(V, F) (V) &= ~(F)

#define OSPF6_SIBLING_DEBUG_MSG 0x01

#define IS_OSPF6_SIBLING_DEBUG_MSG (ospf6_sibling_debug_msg & OSPF6_SIBLING_DEBUG_MSG)

extern unsigned long ospf6_sibling_debug_msg;

void ospf6_sibling_debug_init(void);

#endif
