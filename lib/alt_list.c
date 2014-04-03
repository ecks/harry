#include "assert.h"
#include <stdlib.h>

#include "alt_list.h"

struct alt_list * alt_list_new(void)
{
  return calloc(1, sizeof(struct alt_list));
}

/* Free list. */
void
alt_list_free (struct alt_list *l)
{   
  free(l);
} 

/* Allocate new listnode.  Internal use only. */
static struct alt_listnode *
alt_listnode_new (void)
{   
  return calloc(1, sizeof (struct alt_listnode));
} 

/* Free listnode. */
static void
alt_listnode_free (struct alt_listnode *node)
{ 
  free(node);
}

/*
 * Add a node to the list.  If the list was sorted according to the
 * cmp function, insert a new node with the given val such that the
 * list remains sorted.  The new node is always inserted; there is no
 * notion of omitting duplicates.
 */
void
alt_listnode_add_sort (struct alt_list *alt_list, void *val)
{
  struct alt_listnode *n; 
  struct alt_listnode *new;
        
  assert (val != NULL);
          
  new = alt_listnode_new (); 
  new->data = val;

  if (alt_list->cmp)
  {   
    for (n = alt_list->head; n; n = n->next)
    {   
      if ((*alt_list->cmp) (val, n->data) < 0)
      {    
        new->next = n;
        new->prev = n->prev;

        if (n->prev)
          n->prev->next = new;
        else
          alt_list->head = new;
        n->prev = new;
        alt_list->count++;
        return;
      }
    }
  }

  new->prev = alt_list->tail;

  if (alt_list->tail)
    alt_list->tail->next = new;
  else
    alt_list->head = new;

  alt_list->tail = new;
  alt_list->count++;
}

/* Delete all listnode from the list. */
void
alt_list_delete_all_node (struct alt_list *list)
{
  struct alt_listnode *node;
  struct alt_listnode *next;

  assert(list);
  for (node = list->head; node; node = next)
  {
    next = node->next;
    if (list->del)
      (*list->del) (node->data);
    alt_listnode_free (node);
  }
  list->head = list->tail = NULL;
  list->count = 0;
}

/* Delete all listnode then free list itself. */
void
alt_list_delete (struct alt_list *list)
{
  assert(list);
  alt_list_delete_all_node (list);
  alt_list_free (list);
}

