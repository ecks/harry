#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <limits.h>
#include <stddef.h>
#include <assert.h>
#include <poll.h>

#include "util.h"
#include "dblist.h"
#include "poll-loop.h"

#undef poll_fd_wait

/* An event that will wake the following call to poll_block(). */
struct poll_waiter {
    /* Set when the waiter is created. */
    struct list node;           /* Element in global waiters list. */
    int fd;                     /* File descriptor. */
    short int events;           /* Events to wait for (POLLIN, POLLOUT). */
    const char *where;          /* Where the waiter was created. */

    /* Set only when poll_block() is called. */
    struct pollfd *pollfd;      /* Pointer to element of the pollfds array. */
};

/* All active poll waiters. */
static struct list waiters = LIST_INITIALIZER(&waiters);

/* Time at which to wake up the next call to poll_block(), in milliseconds as
 * returned by time_msec(), LLONG_MIN to wake up immediately, or LLONG_MAX to
 * wait forever. */
static long long int timeout_when = LLONG_MAX;

static struct poll_waiter *new_waiter(int fd, short int events,
                                      const char *where);

/* Registers 'fd' as waiting for the specified 'events' (which should be POLLIN
 * or POLLOUT or POLLIN | POLLOUT).  The following call to poll_block() will
 * wake up when 'fd' becomes ready for one or more of the requested events.
 *
 * The event registration is one-shot: only the following call to poll_block()
 * is affected.  The event will need to be re-registered after poll_block() is
 * called if it is to persist.
 *
 * Ordinarily the 'where' argument is supplied automatically; see poll-loop.h
 * for more information. */
struct poll_waiter *
poll_fd_wait(int fd, short int events, const char *where)
{   
    return new_waiter(fd, events, where);
}

/* Blocks until one or more of the events registered with poll_fd_wait()
 * occurs, or until the minimum duration registered with poll_timer_wait()
 * elapses, or not at all if poll_immediate_wake() has been called. */
void
poll_block(void)
{
    static struct pollfd *pollfds;
    static size_t max_pollfds;

    struct poll_waiter * pw, * next;
    struct list * node;
    int n_waiters, n_pollfds;
    int elapsed;
    int retval;

    if (max_pollfds < n_waiters) {
        max_pollfds = n_waiters;
        pollfds = realloc(pollfds, max_pollfds * sizeof *pollfds);
    }

    n_pollfds = 0;
    LIST_FOR_EACH (pw, struct poll_waiter, node, &waiters) {
        pw->pollfd = &pollfds[n_pollfds];
        pollfds[n_pollfds].fd = pw->fd;
        pollfds[n_pollfds].events = pw->events;
        pollfds[n_pollfds].revents = 0;
        n_pollfds++;
    }

    retval = time_poll(pollfds, n_pollfds, timeout_when, &elapsed);
    if(retval < 0)
    {
      printf("poll: %d\n", strerror(-retval));
    }
    else if(!retval)
    {
      printf("poll timeout\n");
    }

    LIST_FOR_EACH_SAFE(pw, next, struct poll_waiter, node, &waiters)
    {
      if(pw->pollfd->revents)
      {
        printf("poll: revent has occured\n");
      }
      poll_cancel(pw);
    }

    timeout_when = LLONG_MAX;
}

/* Cancels the file descriptor event registered with poll_fd_wait() using 'pw',
 * the struct poll_waiter returned by that function.
 *
 * An event registered with poll_fd_wait() may be canceled from its time of
 * registration until the next call to poll_block().  At that point, the event
 * is automatically canceled by the system and its poll_waiter is freed. */
void
poll_cancel(struct poll_waiter *pw)
{
    if (pw) {
        list_remove(&pw->node);
        free(pw);
    }
}

/* Creates and returns a new poll_waiter for 'fd' and 'events'. */
static struct poll_waiter *
new_waiter(int fd, short int events, const char *where)
{
    struct poll_waiter *waiter = malloc(sizeof *waiter);
    assert(fd >= 0);
    waiter->fd = fd;
    waiter->events = events;
    waiter->where = where;
    list_push_back(&waiters, &waiter->node);
    return waiter;
}
