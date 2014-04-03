#ifndef PQUEUE_H
#define PQUEUE_H

struct pqueue
{
  void ** array;
  int array_size;
  int size;

  int (*cmp) (void *, void *); 
  void (*update) (void * node, int actual_position);
};

#define PQUEUE_INIT_ARRAYSIZE 32

extern struct pqueue *pqueue_create (void);
extern void pqueue_delete (struct pqueue *queue);

extern void pqueue_enqueue (void *data, struct pqueue *queue);
extern void *pqueue_dequeue (struct pqueue *queue);

extern void trickle_down (int index, struct pqueue *queue);
extern void trickle_up (int index, struct pqueue *queue);

#endif
