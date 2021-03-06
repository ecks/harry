#ifndef LIST_H
#define LIST_H

/* Doubly linked list head or element. */
struct list {
    struct list *prev;     /* Previous list element. */
    struct list *next;     /* Next list element. */
    /*  
     * Returns -1 if val1 < val2, 0 if equal?, 1 if val1 > val2.
     * Used as definition of sorted for listnode_add_sort
     */
    int (*cmp) (void * val1, void * val2);
};

#define LIST_INITIALIZER(LIST) { LIST, LIST }

struct list * list_new();
void list_init(struct list *);
void list_poison(struct list *);

void list_insert(struct list *, struct list *);
void list_push_back_sort(struct list *, struct list *);
void list_push_back(struct list *, struct list *);

struct list *list_remove(struct list *);
struct list *list_pop_front(struct list *);
struct list *list_peek_front(struct list * list);

size_t list_size(const struct list *);
bool list_empty(const struct list *);

#define LIST_FOR_EACH(ITER, STRUCT, MEMBER, LIST)                   \
    for (ITER = CONTAINER_OF((LIST)->next, STRUCT, MEMBER);         \
         &(ITER)->MEMBER != (LIST);                                 \
         ITER = CONTAINER_OF((ITER)->MEMBER.next, STRUCT, MEMBER))
#define LIST_FOR_EACH_REVERSE(ITER, STRUCT, MEMBER, LIST)           \
    for (ITER = CONTAINER_OF((LIST)->prev, STRUCT, MEMBER);         \
         &(ITER)->MEMBER != (LIST);                                 \
         ITER = CONTAINER_OF((ITER)->MEMBER.prev, STRUCT, MEMBER))
#define LIST_FOR_EACH_SAFE(ITER, NEXT, STRUCT, MEMBER, LIST)        \
    for (ITER = CONTAINER_OF((LIST)->next, STRUCT, MEMBER);         \
         (NEXT = CONTAINER_OF((ITER)->MEMBER.next, STRUCT, MEMBER), \
          &(ITER)->MEMBER != (LIST));                               \
         ITER = NEXT)


#endif
