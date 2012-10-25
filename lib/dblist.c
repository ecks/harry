#include <string.h>

#include "dblist.h"

/* Initializes 'list' as an empty list. */
void
list_init(struct list *list)
{
    list->next = list->prev = list;
}

/* Initializes 'list' with pointers that will (probably) cause segfaults if
 * dereferenced and, better yet, show up clearly in a debugger. */
void
list_poison(struct list *list)
{
    memset(list, 0xcc, sizeof *list);
}

/* Inserts 'elem' just before 'before'. */
void
list_insert(struct list *before, struct list *elem)
{
    elem->prev = before->prev;
    elem->next = before;
    before->prev->next = elem;
    before->prev = elem;
}

/* Inserts 'elem' at the end of 'list', so that it becomes the back in
 * 'list'. */
void
list_push_back(struct list *list, struct list *elem)
{
    list_insert(list, elem);
}

/* Removes 'elem' from its list and returns the element that followed it.   
   Undefined behavior if 'elem' is not in a list. */
struct list *list_remove(struct list *elem)
{
    elem->prev->next = elem->next;
    elem->next->prev = elem->prev;
    return elem->next;
}

/* Removes the front element from 'list' and returns it.  Undefined behavior if
   'list' is empty before removal. */
struct list *
list_pop_front(struct list *list)
{
  struct list *front = list->next;
  list_remove(front);
  return front;
}

/* Returns the number of elements in 'list'.
   Runs in O(n) in the number of elements. */
size_t
list_size(const struct list *list)
{
    const struct list *e;
    size_t cnt = 0;

    for (e = list->next; e != list; e = e->next)
        cnt++;
    return cnt;
}
