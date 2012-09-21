#ifndef POLL_LOOP_H
#define POLL_LOOP_H

struct poll_waiter;

/* Schedule events to wake up the following poll_block().
 *
 * The poll_loop logs the 'where' argument to each function at "debug" level
 * when an event causes a wakeup.  Ordinarily, it is automatically filled in
 * with the location in the source of the call, and caller should therefore
 * omit it.  But, if the function you are implementing is very generic, so that
 * its location in the source would not be very helpful for debugging, you can
 * avoid the macro expansion and pass a different argument, e.g.:
 *      (poll_fd_wait)(fd, events, where);
 * See timer_wait() for an example.
 */
struct poll_waiter *poll_fd_wait(int fd, short int events, const char *where);
#define poll_fd_wait(fd, events) poll_fd_wait(fd, events, SOURCE_LOCATOR)

/* Wait until an event occurs. */
void poll_block(void);

/* Cancel a file descriptor callback or event. */
void poll_cancel(struct poll_waiter *);

#endif
