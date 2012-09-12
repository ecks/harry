#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <stdbool.h>
#include "Class.h"
#include "list.h"

struct list * list_new()
{
  struct list * new = calloc(1, sizeof(struct list));
  return new;
}

struct listnode * listnode_new(const void * _class, ...)
{
  va_list ap;

  struct listnode * node = calloc(1, sizeof(struct listnode));

  va_start(ap, _class);
  node->data = vnew(_class, &ap);
  va_end(ap);

  return node;
}

void listnode_extract(struct listnode * node, ...)
{
  va_list ap;
 
  va_start(ap, node);
  extract(node->data, &ap);
  va_end(ap);
}

bool listnode_compare_by(struct listnode * node_a, struct listnode * node_b, ...)
{
  va_list ap;

  va_start(ap, node_b);
  compare_by(node_a->data, node_b->data, &ap);
  va_end(ap);
}

struct listnode * listnode_dupl(struct listnode * clone)
{
  struct listnode * node = calloc(1, sizeof(struct listnode));
  node->data = dupl(clone->data);
  return node;
}

void list_extend(struct list * extend_to, struct list * extend_from)
{
  struct listnode * node_extend_from;
  LIST_FOREACH(extend_from, node_extend_from)
  {
    struct listnode * node_extend_to = listnode_dupl(node_extend_from);
    LIST_APPEND(extend_to, node_extend_to);
  } 
} 


void listnode_delete(struct listnode * node)
{
  delete(node->data);
  free(node);
}

struct listnode * list_pop(struct list * list)
{
  struct listnode * head = list->head;
  // remove the head
  if(list->size == 1)
  {
    list->head = list->tail = NULL;
    list->size = 0;
  }
  else
  {
    list->head = head->next;
    list->head->prev = NULL;
    list->size--;
  }

  return head;
}

void list_clear(struct list * list)
{
  struct listnode * node = list->head, *tmp;
  while (node != NULL) 
  {
    tmp=node->next;
    free(node->data);
    free(node);
    node=tmp;
  }
  list->head = list->tail = NULL;
  list->size = 0;
}
