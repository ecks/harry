#ifndef ZEBRALITE_DEBUG
#define ZEBRALITE_DEBUG

#define SET_FLAG(V, F) (V) |= (F)
#define UNSET_FLAG(V, F) (V) &= ~(F)

#define ZEBRALITE_DEBUG_RIB     0x01

#define IS_ZEBRALITE_DEBUG_RIB  (zebralite_debug_rib & ZEBRALITE_DEBUG_RIB)

extern unsigned long zebralite_debug_rib;

#endif
