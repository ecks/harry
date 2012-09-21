#ifndef FATAL_SIGNAL_H
#define FATAL_SIGNAL_H

/* Interface for other code that catches one of our signals and needs to pass
 * it through. */
void fatal_signal_handler(int sig_nr);

#endif
