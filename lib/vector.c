#include <stdlib.h>
#include <string.h>

#include "vector.h"

struct vector * vector_init(unsigned int size)
{
  struct vector * v = calloc(1, sizeof(*v));

  /* allocate at least one slot */
  if (size == 0)
    size = 1;

  v->alloced = size;
  v->active = 0;
  v->index = calloc(1, sizeof (void *) * size);
  return v;
}

void
vector_free (struct vector * v)
{
  free (v->index);
  free (v);
}

/* Check assigned index, and if it runs short double index pointer */
void
vector_ensure (struct vector * v, unsigned int num)
{
  if (v->alloced > num)
    return;

  v->index = realloc(v->index, sizeof (void *) * (v->alloced * 2));
  memset (&v->index[v->alloced], 0, sizeof (void *) * v->alloced);
  v->alloced *= 2;

  if (v->alloced <= num)
    vector_ensure (v, num);
}

/* Set value to specified index slot. */
int
vector_set_index (struct vector * v, unsigned int i, void *val)
{
  vector_ensure (v, i);

  v->index[i] = val;

  if (v->active <= i)
    v->active = i + 1;

  return i;
}

/* Look up vector.  */
void * vector_lookup (struct vector * v, unsigned int i)
{
  if (i >= v->active)
    return NULL;
  return v->index[i];
}
