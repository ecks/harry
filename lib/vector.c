#include <stdlib.h>
#include <string.h>

#include "vector.h"

vector vector_init(unsigned int size)
{
  vector v = calloc(1, sizeof(*v));

  /* allocate at least one slot */
  if (size == 0)
    size = 1;

  v->alloced = size;
  v->active = 0;
  v->index = calloc(1, sizeof (void *) * size);
  return v;
}

void
vector_free (vector v)
{
  free (v->index);
  free (v);
}

vector
vector_copy (vector v)
{
  unsigned int size;
  vector new = calloc(1, sizeof (struct _vector));

  new->active = v->active;
  new->alloced = v->alloced;

  size = sizeof (void *) * (v->alloced);
  new->index = calloc(1, size);
  memcpy (new->index, v->index, size);

  return new;
}

/* Check assigned index, and if it runs short double index pointer */
void
vector_ensure (vector v, unsigned int num)
{
  if (v->alloced > num)
    return;

  v->index = realloc(v->index, sizeof (void *) * (v->alloced * 2));
  memset (&v->index[v->alloced], 0, sizeof (void *) * v->alloced);
  v->alloced *= 2;

  if (v->alloced <= num)
    vector_ensure (v, num);
}

/* This function only returns next empty slot index.  It dose not mean
   the slot's index memory is assigned, please call vector_ensure()
   after calling this function. */
int
vector_empty_slot (vector v)
{
  unsigned int i;

  if (v->active == 0)
    return 0;

  for (i = 0; i < v->active; i++)
    if (v->index[i] == 0)
      return i;

  return i;
}

/* Set value to the smallest empty slot. */
int
vector_set (vector v, void *val)
{
  unsigned int i;

  i = vector_empty_slot (v);
  vector_ensure (v, i);

  v->index[i] = val;

  if (v->active <= i)
    v->active = i + 1;

  return i;
}

/* Set value to specified index slot. */
int
vector_set_index (vector v, unsigned int i, void *val)
{
  vector_ensure (v, i);

  v->index[i] = val;

  if (v->active <= i)
    v->active = i + 1;

  return i;
}

/* Look up vector.  */
void * vector_lookup (vector v, unsigned int i)
{
  if (i >= v->active)
    return NULL;
  return v->index[i];
}
