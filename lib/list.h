#ifndef LIST_H
#define LIST_H

/* Linked list */
struct list
{
        struct listnode * head;
        struct listnode * tail;
        unsigned int size;
};

struct listnode
{
        struct listnode * prev;
        struct listnode * next;
        void * data;
};

#define LIST_APPEND(list,node) { if(!list->head){list->head = list->tail = node;node->prev = node->next = NULL;list->size=1;}else{node->prev=list->tail;node->next=NULL;list->tail->next=node;list->tail=node;list->size++;} }
#define FREE_LINKED_LIST(list) { \
        struct listnode * node = list->head, *tmp;\
        while (node != NULL) {\
                tmp=node->next;\
                free(node->data);\
                free(node);\
                node=tmp;\
        }\
        free(list); }
#define FREE_LINKED_LIST_NOT_DATA(list) { \
        struct listnode * node = list->head, *tmp;\
        while (node != NULL) {\
                tmp=node->next;\
                free(node);\
                node=tmp;\
        }\
        free(list); }
#define LIST_FOREACH(list,node) for(node = list->head; node != NULL; node = node->next)

#define LIST_POP(list, val) { \
        struct listnode * head = list->head;\
        if(list->size == 1) {\
          list->head = list->tail = NULL;\
          list->size = 0;\
        }\
        else {\
          list->head = head->next;\
          list->head->prev = NULL;\
          list->size--;\
       }\
       val=head;}

struct list * list_new();
struct listnode * listnode_new(const void * _class, ...);
void listnode_extract(struct listnode * node, ...);
bool listnode_compare_by(struct listnode * node_a, struct listnode * node_b, ...);
struct listnode * listnode_dupl(struct listnode * clone);
void list_extend(struct list * extend_to, struct list * extend_from);
void listnode_delete(struct listnode * node);
struct listnode * list_pop(struct list * list);
void list_clear(struct list * list);

#endif
