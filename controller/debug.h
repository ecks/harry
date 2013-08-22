#ifndef CONTROLLER_DEBUG
#define CONTROLLER_DEBUG

#define SET_FLAG(V, F) (V) |= (F)
#define UNSET_FLAG(V, F) (V) &= ~(F)

#define CONTROLLER_DEBUG_MSG 0x01

#define IS_CONTROLLER_DEBUG_MSG (controller_debug_msg & CONTROLLER_DEBUG_MSG)

extern unsigned long controller_debug_msg;

void controller_debug_init(void);

#endif
