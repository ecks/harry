#include "config.h"
#include "errno.h"

#include "stdlib.h"
#include "string.h"
#include "stdio.h"
#include "assert.h"
#include "sys/select.h"
#include <time.h>

#include "thread.h"

/* Recent absolute time of day */
struct timeval recent_time;
/* Relative time, since startup */
static struct timeval relative_time;

/* Struct timeval's tv_usec one second value.  */
#define TIMER_SECOND_MICRO 1000000L

/* Adjust so that tv_usec is in the range [0,TIMER_SECOND_MICRO).
 *    And change negative values to 0. */
static struct timeval
timeval_adjust (struct timeval a)
{
  while (a.tv_usec >= TIMER_SECOND_MICRO)
  {
    a.tv_usec -= TIMER_SECOND_MICRO;
    a.tv_sec++;
  }

  while (a.tv_usec < 0)
  {
    a.tv_usec += TIMER_SECOND_MICRO;
    a.tv_sec--;
  }

  if (a.tv_sec < 0)
  /* Change negative timeouts to 0. */
    a.tv_sec = a.tv_usec = 0;

  return a;
}

static struct timeval
timeval_subtract (struct timeval a, struct timeval b)
{
  struct timeval ret;

  ret.tv_usec = a.tv_usec - b.tv_usec;
  ret.tv_sec = a.tv_sec - b.tv_sec;

  return timeval_adjust (ret);
}

static long
timeval_cmp (struct timeval a, struct timeval b)
{
    return (a.tv_sec == b.tv_sec
                  ? a.tv_usec - b.tv_usec : a.tv_sec - b.tv_sec);
}

/* gettimeofday wrapper, to keep recent_time updated */
static int 
zebralite_gettimeofday(struct timeval * tv)
{
  int ret;

  assert(tv);

  if(!(ret = gettimeofday(&recent_time, NULL)))
  {
    /* avoid copy if user passed recent_time pointer.. */
    if(tv != &recent_time)
      *tv = recent_time;
    return 0;
  }

  return ret;
}

static int zebralite_get_relative(struct timeval * tv)
{
  int ret; 

#ifdef HAVE_CLOCK_MONOTONIC
    {
      struct timespec tp;
      if (!(ret = clock_gettime (CLOCK_MONOTONIC, &tp)))
      {    
        relative_time.tv_sec = tp.tv_sec;
        relative_time.tv_usec = tp.tv_nsec / 1000;
      }    
    }
#else /* !HAVE_CLOCK_MONOTONIC */
//      if (!(ret = quagga_gettimeofday (&recent_time)))
//            quagga_gettimeofday_relative_adjust();
#endif /* HAVE_CLOCK_MONOTONIC */

  if (tv) 
    *tv = relative_time;

  return ret;
}

int zebralite_gettime(enum zebralite_clkid clkid, struct timeval * tv)
{
  switch (clkid)
  {
    case ZEBRALITE_CLK_REALTIME:
      return zebralite_gettimeofday(tv);
    case ZEBRALITE_CLK_MONOTONIC:
      return zebralite_get_relative (tv);
    default:
      errno = EINVAL;
      return -1;
  }      
}

struct thread_master * thread_master_create()
{
  return (struct thread_master *)calloc(1, sizeof(struct thread_master));
}

/* Add a new thread to the list.  */
static void
thread_list_add (struct thread_list *list, struct thread *thread)
{
  thread->next = NULL;
  thread->prev = list->tail;
  if (list->tail)
    list->tail->next = thread;
  else
    list->head = thread;
  list->tail = thread;
  list->count++;
}

/* Add a new thread just before the point.  */
static void
thread_list_add_before (struct thread_list *list,
                        struct thread *point,
                        struct thread *thread)
{
  thread->next = point;
  thread->prev = point->prev;
  if (point->prev)
    point->prev->next = thread;
  else
    list->head = thread;
  point->prev = thread;
  list->count++;
}


/* Delete a thread from the list. */
static struct thread *
thread_list_delete (struct thread_list *list, struct thread *thread)
{
  if (thread->next)
    thread->next->prev = thread->prev;
  else
    list->tail = thread->prev;
  if (thread->prev)
    thread->prev->next = thread->next;
  else
    list->head = thread->next;
  thread->next = thread->prev = NULL;
  list->count--;
  return thread;
}

/* Move thread to unuse list. */
static void
thread_add_unuse (struct thread_master *m, struct thread *thread)
{
  assert (m != NULL && thread != NULL);
  assert (thread->next == NULL);
  assert (thread->prev == NULL);
  assert (thread->type == THREAD_UNUSED);
  thread_list_add (&m->unuse, thread);
  /* XXX: Should we deallocate funcname here? */
}

/* Thread list is empty or not.  */
static inline int
thread_empty (struct thread_list *list)
{
  return  list->head ? 0 : 1;
}

/* Delete top of the list and return it. */
static struct thread *
thread_trim_head (struct thread_list *list)
{
  if (!thread_empty (list))
    return thread_list_delete (list, list->head);
  return NULL;
}

/* Trim blankspace and "()"s */
static char *
strip_funcname (const char *funcname)
{
  char buff[100];
  char tmp, *ret, *e, *b = buff;

  strncpy(buff, funcname, sizeof(buff));
  buff[ sizeof(buff) -1] = '\0';
  e = buff +strlen(buff) -1;

  /* Wont work for funcname ==  "Word (explanation)"  */

  while (*b == ' ' || *b == '(')
    ++b;
  while (*e == ' ' || *e == ')')
    --e;
  e++;

  tmp = *e;
  *e = '\0';
  ret  = strdup(b);
  *e = tmp;

  return ret;
}

/* Get new thread.  */
static struct thread * thread_get (struct thread_master *m, u_char type, int (*func) (struct thread *), void *arg, const char* funcname)
{
  struct thread *thread;

  if (!thread_empty (&m->unuse))
    {
      thread = thread_trim_head (&m->unuse);
      if (thread->funcname)
        free(thread->funcname);
    }
  else
    {
      thread = calloc (1, sizeof (struct thread));
      m->alloc++;
    }
  thread->type = type;
  thread->add_type = type;
  thread->master = m;
  thread->func = func;
  thread->arg = arg;

  thread->funcname = strip_funcname(funcname);

  return thread; 
}

/* Add new read thread. */
struct thread * funcname_thread_add_read(struct thread_master * m, int (*func) (struct thread *), void *arg, int fd, const char* funcname)
{
  struct thread * thread;
 
  assert(m != NULL);

  if(FD_ISSET(fd, &m->readfd))
  {
    printf("There is already a read fd set: %d\n", fd);
    return NULL;
  }

  thread = thread_get(m, THREAD_READ, func, arg, funcname);
  FD_SET(fd, &m->readfd);
  thread->u.fd = fd;
  thread_list_add(&m->read, thread);

  return thread;
}

static struct thread *
funcname_thread_add_timer_timeval (struct thread_master *m,
                                   int (*func) (struct thread *),  
                                   int type,
                                   void *arg, 
                                   struct timeval *time_relative, 
                                   const char* funcname)
{
  struct thread *thread;
  struct thread_list *list;
  struct timeval alarm_time;
  struct thread *tt; 

  assert (m != NULL);

  assert (type == THREAD_TIMER || type == THREAD_BACKGROUND);
  assert (time_relative);
      
  list = ((type == THREAD_TIMER) ? &m->timer : &m->background);
  thread = thread_get (m, type, func, arg, funcname);

  /* Do we need jitter here? */
  zebralite_get_relative (NULL);
  alarm_time.tv_sec = relative_time.tv_sec + time_relative->tv_sec;
  alarm_time.tv_usec = relative_time.tv_usec + time_relative->tv_usec;
  thread->u.sands = timeval_adjust(alarm_time);

  /* Sort by timeval. */
  for (tt = list->head; tt; tt = tt->next)
    if (timeval_cmp (thread->u.sands, tt->u.sands) <= 0)
      break;

  if (tt)
    thread_list_add_before (list, tt, thread);
  else
    thread_list_add (list, thread);

  return thread;
}

/* Add timer event thread. */
struct thread *
funcname_thread_add_timer (struct thread_master *m,
                           int (*func) (struct thread *),  
                           void *arg, long timer, const char* funcname)
{
    struct timeval trel;

    assert (m != NULL);

    trel.tv_sec = timer;
    trel.tv_usec = 0; 

    return funcname_thread_add_timer_timeval (m, func, THREAD_TIMER, arg, 
                                              &trel, funcname);
}

/* Add simple event thread. */
struct thread * funcname_thread_add_event (struct thread_master *m, int (*func) (struct thread *), void *arg, int val, const char* funcname)
{
  struct thread *thread;

  assert (m != NULL);

  thread = thread_get (m, THREAD_EVENT, func, arg, funcname);
  thread->u.val = val;
  thread_list_add (&m->event, thread);

  return thread;
}

/* Cancel thread from scheduler. */
void
thread_cancel (struct thread *thread)
{
  struct thread_list *list;

  switch (thread->type)
    {
    case THREAD_READ:
      assert (FD_ISSET (thread->u.fd, &thread->master->readfd));
      FD_CLR (thread->u.fd, &thread->master->readfd);
      list = &thread->master->read;
      break;
    case THREAD_WRITE:
      assert (FD_ISSET (thread->u.fd, &thread->master->writefd));
      FD_CLR (thread->u.fd, &thread->master->writefd);
      list = &thread->master->write;
      break;
    case THREAD_TIMER:
      list = &thread->master->timer;
      break;
    case THREAD_EVENT:
      list = &thread->master->event;
      break;
    case THREAD_READY:
      list = &thread->master->ready;
      break;
    case THREAD_BACKGROUND:
      list = &thread->master->background;
      break;
    default:
      return;
      break;
    }
  thread_list_delete (list, thread);
  thread->type = THREAD_UNUSED;
  thread_add_unuse (thread->master, thread);
}

static struct timeval * thread_timer_wait (struct thread_list *tlist, struct timeval *timer_val)
{
  if (!thread_empty (tlist))
  {
    *timer_val = timeval_subtract (tlist->head->u.sands, relative_time);
    return timer_val;
  }
  return NULL;
}

static struct thread *
thread_run (struct thread_master *m, struct thread *thread,
            struct thread *fetch)
{
  *fetch = *thread;
  thread->type = THREAD_UNUSED;
  thread->funcname = NULL;  /* thread_call will free fetch's copied pointer */
  thread_add_unuse (m, thread);
  return fetch;
}

static int thread_process_fd(struct thread_list * list, fd_set * fdset, fd_set * mfdset)
{
  struct thread * thread;
  struct thread * next;
  int ready = 0;

  assert(list);

  for(thread = list->head; thread; thread = next)
  {
    next = thread->next;

    if (FD_ISSET (THREAD_FD (thread), fdset))
    {
      assert (FD_ISSET (THREAD_FD (thread), mfdset));
      FD_CLR(THREAD_FD (thread), mfdset);
      thread_list_delete (list, thread);
      thread_list_add (&thread->master->ready, thread);
      thread->type = THREAD_READY;
      ready++;
    }
  }
  return ready;
}

/* Add all timers that have popped to the ready list. */
static unsigned int
thread_timer_process (struct thread_list *list, struct timeval *timenow)
{
  struct thread *thread;
  unsigned int ready = 0;

  for (thread = list->head; thread; thread = thread->next)
  {
    if (timeval_cmp (*timenow, thread->u.sands) < 0)
      return ready;
    thread_list_delete (list, thread);
    thread->type = THREAD_READY;
    thread_list_add (&thread->master->ready, thread);
    ready++;
  }
  return ready;
}

/* Fetch next ready thread. */
struct thread * thread_fetch(struct thread_master * m, struct thread * fetch)
{
  printf("inside thread_fetch\n");
  struct thread * thread;
  fd_set readfd;
  fd_set writefd;
  fd_set exceptfd;
  struct timeval timer_val;
  struct timeval timer_val_bg;
  struct timeval * timer_wait;
  struct timeval * timer_wait_bg;

  while(1)
  {
    int num = 0;

    // TODO: set signal handler for events which are highest priority 

    /* Normal events are the second highest priority */
    if((thread = thread_trim_head(&m->event)) != NULL)
      return thread_run(m, thread, fetch);

    /* If there are any ready threads from previous scheduler runs,
     * process top of them.  
     */
    if ((thread = thread_trim_head (&m->ready)) != NULL)
      return thread_run (m, thread, fetch);

    /* Structure copy */
    readfd = m->readfd;
    writefd = m->writefd;
    exceptfd = m->exceptfd;

    /* Calculate select wait timer if nothing else to do */
    zebralite_get_relative(NULL); 
    timer_wait = thread_timer_wait(&m->timer, &timer_val);
    timer_wait_bg = thread_timer_wait(&m->background, &timer_val_bg);

    if (timer_wait_bg &&
      (!timer_wait || (timeval_cmp (*timer_wait, *timer_wait_bg) > 0)))
    timer_wait = timer_wait_bg;

    zlog_debug("about to call select");
    num = select(FD_SETSIZE, &readfd, &writefd, &exceptfd, timer_wait);
    zlog_debug("called select");

    zebralite_get_relative(NULL);
    thread_timer_process (&m->timer, &relative_time);

    if(num < 0)
    {
      zlog_debug("select num: %d", num); 
      perror("select");
    }
 
    if(num > 0)
    {
      thread_process_fd(&m->read, &readfd, &m->readfd);
      thread_process_fd(&m->write, &writefd, &m->readfd);
    }

    if ((thread = thread_trim_head (&m->ready)) != NULL)
      return thread_run (m, thread, fetch);
  }
}

void thread_call(struct thread * thread)
{
  (*thread->func) (thread);

  free(thread->funcname);
}

/* Execute thread */
struct thread * funcname_thread_execute(struct thread_master * m,
                                        int (*func)(struct thread *),
                                        void * arg,
                                        int val,
                                        const char * funcname)
{
  struct thread dummy;

  memset (&dummy, 0, sizeof (struct thread));

  dummy.type = THREAD_EVENT;
  dummy.add_type = THREAD_EXECUTE;
  dummy.master = NULL;
  dummy.func = func;
  dummy.arg = arg;
  dummy.u.val = val;
  dummy.funcname = strip_funcname(funcname);
  thread_call(&dummy);

//  free(dummy.funcname);

  return NULL;
}
