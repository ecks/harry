#ifndef VECTOR_H
#define VECTOR_H

/* struct for vector */
struct vector
{
  unsigned int active;          /* number of active slots */
  unsigned int alloced;         /* number of allocated slot */
  void **index;                 /* index to data */
};

extern struct vector * vector_init (unsigned int size);
void vector_free (struct vector * v);
void vector_ensure (struct vector * v, unsigned int num);
int vector_set_index (struct vector * v, unsigned int i, void *val);
extern void * vector_lookup (struct vector * v, unsigned int);

#endif
