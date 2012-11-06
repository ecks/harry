#include "stdlib.h"
#include "string.h"
#include "stdio.h"
#include "assert.h"
#include "sys/select.h"

#include "thread.h"

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

struct thread * funcname_thread_add_read(struct thread_master * m, int (*func) (struct thread *), void *arg, int fd, const char* funcname)
{
  struct thread * thread;
 
  assert(m != NULL);

  if(FD_ISSET(fd, &m->readfd))
  {
    printf("There is already a read fd set\n");
    return NULL;
  }

  thread = thread_get(m, THREAD_READ, func, arg, funcname);
  FD_SET(fd, &m->readfd);
  thread->u.fd = fd;
  thread_list_add(&m->read, thread);

  return thread;
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

    num = select(FD_SETSIZE, &readfd, &writefd, &exceptfd, NULL);

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
