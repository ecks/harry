#ifndef ALT_LIST
#define ALT_LIST

struct alt_listnode
{
  struct alt_listnode * next;
  struct alt_listnode * prev;

  void * data;
};

struct alt_list
{
  struct alt_listnode * head;
  struct alt_listnode * tail;

  unsigned int count;

  int (*cmp) (void * val1, void * val2);

  void (*del) (void *val);
};

extern struct alt_list *alt_list_new(void);
extern void alt_listnode_add_sort (struct alt_list *, void *);
extern void alt_list_delete (struct alt_list *list);

#endif
