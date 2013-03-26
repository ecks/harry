#include "stdlib.h"
#include "string.h"
#include "stdio.h"
#include "assert.h"
#include "sys/select.h"
#include <sys/time.h>

#include "thread.h"

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

static long
timeval_cmp (struct timeval a, struct timeval b)
{
    return (a.tv_sec == b.tv_sec
                  ? a.tv_usec - b.tv_usec : a.tv_sec - b.tv_sec);
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

/* Fetch next ready thread. */
struct thread * thread_fetch(struct thread_master * m, struct thread * fetch)
{
  printf("inside thread_fetch\n");
  struct thread * thread;
  fd_set readfd;
  fd_set writefd;
  fd_set exceptfd;

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

    printf("about to call select\n");
    num = select(FD_SETSIZE, &readfd, &writefd, &exceptfd, NULL);
    printf("called select\n");

    if(num < 0)
    {
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
