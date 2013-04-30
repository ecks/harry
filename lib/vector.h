#ifndef VECTOR_H
#define VECTOR_H

/* struct for vector */
struct _vector
{
  unsigned int active;          /* number of active slots */
  unsigned int alloced;         /* number of allocated slot */
  void **index;                 /* index to data */
};
typedef struct _vector * vector;

#define VECTOR_MIN_SIZE 1

/* Reference slot at given index, caller must ensure slot is active */
#define vector_slot(V,I)  ((V)->index[(I)])
/* Number of active slots. 
 * Note that this differs from vector_count() as it the count returned
 * will include any empty slots
 */
#define vector_active(V) ((V)->active)


extern vector vector_init (unsigned int size);
void vector_free (vector v);
extern vector vector_copy (vector v);

void vector_ensure (vector v, unsigned int num);
int vector_set_index (vector v, unsigned int i, void *val);
extern void * vector_lookup (vector v, unsigned int);

#endif
