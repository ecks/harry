#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>

#include "fatal-signal.h"

/* Hooks to call upon catching a signal */
struct hook {
    void (*func)(void *aux);
    void *aux;
    bool run_at_exit;
};
#define MAX_HOOKS 32
static struct hook hooks[MAX_HOOKS];
static size_t n_hooks;

static void call_hooks(int sig_nr);

/* Handles fatal signal number 'sig_nr'.
 *
 * Ordinarily this is the actual signal handler.  When other code needs to
 * handle one of our signals, however, it can register for that signal and, if
 * and when necessary, call this function to do fatal signal processing for it
 * and terminate the process.  Currently only timeval.c does this, for SIGALRM.
 * (It is not important whether the other code sets up its signal handler
 * before or after this file, because this file will only set up a signal
 * handler in the case where the signal has its default handling.)  */
void
fatal_signal_handler(int sig_nr)
{
    call_hooks(sig_nr);

    /* Re-raise the signal with the default handling so that the program
     * termination status reflects that we were killed by this signal */
    signal(sig_nr, SIG_DFL);
    raise(sig_nr);
}

static void
call_hooks(int sig_nr)
{
    static volatile sig_atomic_t recurse = 0;
    if (!recurse) {
        size_t i;

        recurse = 1;

        for (i = 0; i < n_hooks; i++) {
            struct hook *h = &hooks[i];
            if (sig_nr || h->run_at_exit) {
                h->func(h->aux);
            }
        }
    }
}
