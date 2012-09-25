#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/stropts.h>
#include <poll.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>

#include "timeval.h"

/* Initialized? */
static bool inited;

/* Has a timer tick occurred? */
static volatile sig_atomic_t tick;

/* The current time, as of the last refresh. */
static struct timeval now;

/* Time at which to die with SIGALRM (if not TIME_MIN). */
static time_t deadline = TIME_MIN;

static void sigalrm_handler(int);
static void refresh_if_ticked(void);
static void block_sigalrm(sigset_t *);
static void unblock_sigalrm(const sigset_t *);

/* Initializes the timetracking module. */
void
time_init(void)
{
    struct sigaction sa;
    struct itimerval itimer;

    if (inited) {
        return;
    }
       inited = true;
    gettimeofday(&now, NULL);
    tick = false;

    /* Set up signal handler. */
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = sigalrm_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
//    if (sigaction(SIGALRM, &sa, NULL)) {
//        perror("sigaction(SIGALRM) failed");
//    }

    /* Set up periodic timer. */
    itimer.it_interval.tv_sec = 0;
    itimer.it_interval.tv_usec = TIME_UPDATE_INTERVAL * 1000;
    itimer.it_value = itimer.it_interval;
//    if (setitimer(ITIMER_REAL, &itimer, NULL)) {
//        perror("setitimer failed");
//    }
}
/* Forces a refresh of the current time from the kernel.  It is not usually
 * necessary to call this function, since the time will be refreshed
 * automatically at least every TIME_UPDATE_INTERVAL milliseconds. */
void
time_refresh(void)
{
    gettimeofday(&now, NULL);
    tick = false;
}

/* Returns the current time, in ms (within TIME_UPDATE_INTERVAL ms). */
long long int
time_msec(void)
{
    refresh_if_ticked();
    return (long long int) now.tv_sec * 1000 + now.tv_usec / 1000;
}

/* Like poll(), except:
 *
 *      - On error, returns a negative error code (instead of setting errno).
 *
 *      - If interrupted by a signal, retries automatically until the original
 *        'timeout' expires.  (Because of this property, this function will
 *        never return -EINTR.)
 *
 *      - As a side effect, refreshes the current time (like time_refresh()).
 */
int
time_poll(struct pollfd *pollfds, int n_pollfds, long long int timeout_when, int * elapsed)
{
  long long int start;
  sigset_t oldsigs;
  bool blocked;
  int retval;

  time_refresh();
  start = time_msec();
  blocked = false;
  for (;;) {
      long long int now = time_msec();
      int time_left;

      if (now >= timeout_when) {
          time_left = 0;
          printf("time_left: 0\n");
      } else if ((unsigned long long int) timeout_when - now > INT_MAX) {
          time_left = INT_MAX;
          printf("time_left: INT_MAX\n");
      } else {
          time_left = timeout_when - now;
          printf("time_left: %d\n", time_left);
      }

      retval = poll(pollfds, n_pollfds, time_left);
      printf("time_poll -> return from poll: %d\n", retval);
      if (retval < 0) {
          retval = -errno;
          perror("time_poll");
      }
      if (retval != -EINTR) {
          break;
      }

      if (!blocked && deadline == TIME_MIN) {
//          block_sigalrm(&oldsigs);
          blocked = true;
      }
      time_refresh();
  }
  if (blocked) {
//      unblock_sigalrm(&oldsigs);
  }
  return retval;
}

static void
refresh_if_ticked(void)
{
    assert(inited);
    if (tick) {
        time_refresh();
    }
}

static void
sigalrm_handler(int sig_nr)
{
    tick = true;
    printf("handled signal: %d\n", sig_nr);
    if (deadline != TIME_MIN && time(0) > deadline) {
        printf("calling fatal_signal_handler\n");
        fatal_signal_handler(sig_nr);
    }
}

static void
block_sigalrm(sigset_t *oldsigs)
{
    sigset_t sigalrm;
    sigemptyset(&sigalrm);
    sigaddset(&sigalrm, SIGALRM);
    if (sigprocmask(SIG_BLOCK, &sigalrm, oldsigs)) {
        perror("sigprocmask");
    }
}

static void
unblock_sigalrm(const sigset_t *oldsigs)
{
    if (sigprocmask(SIG_SETMASK, oldsigs, NULL)) {
        perror("sigprocmask");
    }
}
