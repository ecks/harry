#ifndef TIMEVAL_H
#define TIMEVAL_H

#include "type-props.h"

#define TIME_MIN TYPE_MINIMUM(time_t)

/* Interval between updates to the time reported by time_gettimeofday(), in ms.
 * This should not be adjusted much below 10 ms or so with the current
 * implementation, or too much time will be wasted in signal handlers and calls
 * to time(0). */
#define TIME_UPDATE_INTERVAL 100

void time_init(void);
int time_poll(struct pollfd *, int n_pollfds, long long int timeout_when, int * elapsed);

#endif
