#ifndef THREAD_H
#define THREAD_H

#define thread_add_read(m,f,a,v) funcname_thread_add_read(m,f,a,v,#f)
#define thread_add_timer(m,f,a,v) funcname_thread_add_timer(m,f,a,v,#f)
#define thread_add_event(m,f,a,v) funcname_thread_add_event(m,f,a,v,#f)


/* Linked list of thread. */
struct thread_list
{
  struct thread * head;
  struct thread * tail;
  int count;
};

struct thread_master
{
  struct thread_list read;
  struct thread_list write;
  struct thread_list timer;
  struct thread_list event;
  struct thread_list ready;
  struct thread_list unuse;
  struct thread_list background;
  fd_set readfd;
  fd_set writefd;
  fd_set exceptfd;
  unsigned long alloc;
};

typedef unsigned char thread_type;

struct thread
{
  thread_type type;
  thread_type add_type;
  struct thread * next;
  struct thread * prev;
  struct thread_master * master;
  int (*func) (struct thread *);
  void * arg;
  union
  {
    int val;
    int fd;
    struct timeval sands;
  } u;

  char * funcname;
};

/* Thread types. */
#define THREAD_READ           0
#define THREAD_WRITE          1
#define THREAD_TIMER          2
#define THREAD_EVENT          3
#define THREAD_READY          4
#define THREAD_BACKGROUND     5
#define THREAD_UNUSED         6
#define THREAD_EXECUTE        7

/* Macros. */
#define THREAD_ARG(X) ((X)->arg)
#define THREAD_FD(X)  ((X)->u.fd)
#define THREAD_VAL(X) ((X)->u.val)

#define THREAD_OFF(thread) \
  do { \
    if (thread) \
      { \
        thread_cancel (thread); \
        thread = NULL; \
      } \
  } while (0)

/* Prototypes. */
extern struct thread_master *thread_master_create (void);
extern struct thread * funcname_thread_add_read (struct thread_master *,
                                                int (*)(struct thread *),
                                                void *, int, const char*);
extern struct thread *funcname_thread_add_timer (struct thread_master *,
                                                int (*)(struct thread *), 
                                                void *, long, const char*);
extern struct thread * funcname_thread_add_event (struct thread_master *,
                                                 int (*)(struct thread *),
                                                 void *, int, const char*);
extern void thread_cancel (struct thread *);
extern struct thread * thread_fetch (struct thread_master *, struct thread *);
extern void thread_call (struct thread *);

#endif
