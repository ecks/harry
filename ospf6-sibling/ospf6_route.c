#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <netinet/in.h>

#include "util.h"
#include "dblist.h"
#include "thread.h"
#include "prefix.h"
#include "table.h"
#include "ospf6_route.h"


void
ospf6_linkstate_prefix (u_int32_t adv_router, u_int32_t id,
                            struct prefix *prefix)
{
  memset (prefix, 0, sizeof (struct prefix));
  prefix->family = AF_INET6;
  prefix->prefixlen = 64;
  memcpy (&prefix->u.prefix6.s6_addr[0], &adv_router, 4);
  memcpy (&prefix->u.prefix6.s6_addr[4], &id, 4);
}


void
ospf6_linkstate_prefix2str (struct prefix *prefix, char *buf, int size)
{
  u_int32_t adv_router, id;
  char adv_router_str[16], id_str[16];
  memcpy (&adv_router, &prefix->u.prefix6.s6_addr[0], 4);
  memcpy (&id, &prefix->u.prefix6.s6_addr[4], 4);
  inet_ntop (AF_INET, &adv_router, adv_router_str, sizeof (adv_router_str));
  inet_ntop (AF_INET, &id, id_str, sizeof (id_str));
  if (ntohl (id))
    snprintf (buf, size, "%s Net-ID: %s", adv_router_str, id_str);
  else 
    snprintf (buf, size, "%s", adv_router_str);
}

struct ospf6_route * ospf6_route_create(void)
{
  struct ospf6_route * route;
  route = calloc(1, sizeof(struct ospf6_route));
  return route;
}

void ospf6_route_delete(struct ospf6_route * route)
{
  free(route);
}

struct ospf6_route * ospf6_route_copy(struct ospf6_route * route)
{
  struct ospf6_route *new;

  new = ospf6_route_create ();
  memcpy (new, route, sizeof (struct ospf6_route));
  new->rnode = NULL;
  new->prev = NULL;
  new->next = NULL;
  new->table = NULL;
  new->lock = 0; 
  return new; 
}

void ospf6_route_lock(struct ospf6_route * route)
{
  route->lock++;
}

void
ospf6_route_unlock (struct ospf6_route *route)
{
  assert (route->lock > 0);
  route->lock--;
  if (route->lock == 0)
  {    
    /* Can't detach from the table until here
    *  because ospf6_route_next () will use
    *  the 'route->table' pointer for logging */
    route->table = NULL;
    ospf6_route_delete (route);
  }    
}

/* Route compare function. If ra is more preferred, it returns
   less than 0. If rb is more preferred returns greater than 0.
   Otherwise (neither one is preferred), returns 0 */
static int
ospf6_route_cmp (struct ospf6_route *ra, struct ospf6_route *rb)
{
  assert (ospf6_route_is_same (ra, rb));
  assert (OSPF6_PATH_TYPE_NONE < ra->path.type &&
          ra->path.type < OSPF6_PATH_TYPE_MAX);
  assert (OSPF6_PATH_TYPE_NONE < rb->path.type &&
          rb->path.type < OSPF6_PATH_TYPE_MAX);

  if (ra->type != rb->type)
    return (ra->type - rb->type);

//  if (ra->path.area_id != rb->path.area_id)
//    return (ntohl (ra->path.area_id) - ntohl (rb->path.area_id));

  if (ra->path.type != rb->path.type)
    return (ra->path.type - rb->path.type);

  if (ra->path.type == OSPF6_PATH_TYPE_EXTERNAL2)
  {
    if (ra->path.cost_e2 != rb->path.cost_e2)
      return (ra->path.cost_e2 - rb->path.cost_e2);
  }
  else
  {
    if (ra->path.cost != rb->path.cost)
      return (ra->path.cost - rb->path.cost);
  }

  return 0;
}

struct ospf6_route * ospf6_route_lookup (struct prefix *prefix,
                                         struct ospf6_route_table *table)
{
  struct route_node *node;
  struct ospf6_route *route;

  node = route_node_lookup (table->table, prefix);
  if (node == NULL)
    return NULL;

  route = (struct ospf6_route *) node->info;
  return route;
}

#ifndef NDEBUG
static void 
route_table_assert (struct ospf6_route_table *table)
{
  struct ospf6_route *prev, *r, *next;
  char buf[64];
  unsigned int link_error = 0, num = 0; 
          
  r = ospf6_route_head (table);
  prev = NULL;
  while (r)
  {    
    if (r->prev != prev)
      link_error++;
                                 
    next = ospf6_route_next (r); 
                                       
    if (r->next != next)
      link_error++;
                                             
    prev = r; 
    r = next;
  }    
                
  for (r = ospf6_route_head (table); r; r = ospf6_route_next (r)) 
    num++;
                  
  if (link_error == 0 && num == table->count)
    return;

  zlog_err ("PANIC !!");
  zlog_err ("Something has gone wrong with ospf6_route_table[%p]", table);
  zlog_debug ("table count = %d, real number = %d", table->count, num);
  zlog_debug ("DUMP START");
  for (r = ospf6_route_head (table); r; r = ospf6_route_next (r)) 
  {    
    prefix2str (&r->prefix, buf, sizeof (buf));
    zlog_info ("%p<-[%p]->%p : %s", r->prev, r, r->next, buf);
  }    
  zlog_debug ("DUMP END");

  assert (link_error == 0 && num == table->count);
}
#define ospf6_route_table_assert(t) (route_table_assert (t))
#else
#define ospf6_route_table_assert(t) ((void) 0)
#endif /*NDEBUG*/

struct ospf6_route * ospf6_route_add(struct ospf6_route * route, struct ospf6_route_table * table)
{
  struct route_node * node, * nextnode, * prevnode;
  struct ospf6_route *current = NULL;
  struct ospf6_route *prev = NULL, *old = NULL, *next = NULL;
  char buf[64];
  struct timeval now;

  assert (route->rnode == NULL);
  assert (route->lock == 0);
  assert (route->next == NULL);
  assert (route->prev == NULL);

  if (route->type == OSPF6_DEST_TYPE_LINKSTATE)
    ospf6_linkstate_prefix2str (&route->prefix, buf, sizeof (buf));
  else
    prefix2str (&route->prefix, buf, sizeof (buf));

  zebralite_gettime(ZEBRALITE_CLK_MONOTONIC, &now);

  node = route_node_get (table->table, &route->prefix);
  route->rnode = node;

  /* find place to insert */
  for (current = node->info; current; current = current->next)
  {   
    if (! ospf6_route_is_same (current, route))
      next = current;
    else if (current->type != route->type)
      prev = current;
    else if (ospf6_route_is_same_origin (current, route))
      old = current;
    else if (ospf6_route_cmp (current, route) > 0)
      next = current;
    else
      prev = current;

    if (old || next)
      break;
  }

  if (old)
  {
    /* if route does not actually change, return unchanged */
    if (ospf6_route_is_identical (old, route))
    {
      ospf6_route_delete (route);
      SET_FLAG (old->flag, OSPF6_ROUTE_ADD);
      ospf6_route_table_assert (table);

      return old;
    }

    /* replace old one if exists */
    if (node->info == old)
    {
      node->info = route;
      SET_FLAG (route->flag, OSPF6_ROUTE_BEST);
    }

    if (old->prev)
      old->prev->next = route;
    route->prev = old->prev;
    if (old->next)
      old->next->prev = route;
    route->next = old->next;

    route->installed = old->installed;
    route->changed = now;
    assert (route->table == NULL);
    route->table = table;

    ospf6_route_unlock (old); /* will be deleted later */
    ospf6_route_lock (route);

    SET_FLAG (route->flag, OSPF6_ROUTE_CHANGE);
    ospf6_route_table_assert (table);

    if (table->hook_add)
      (*table->hook_add) (route);

    return route;
  }

  /* insert if previous or next node found */
  if (prev || next)
  {
    if (prev == NULL)
      prev = next->prev;
    if (next == NULL)
      next = prev->next;

    if (prev)
      prev->next = route;
    route->prev = prev;
    if (next)
      next->prev = route;
    route->next = next;

    if (node->info == next)
    {
      assert (next->rnode == node);
      node->info = route;
      UNSET_FLAG (next->flag, OSPF6_ROUTE_BEST);
      SET_FLAG (route->flag, OSPF6_ROUTE_BEST);
    }

    route->installed = now;
    route->changed = now;
    assert (route->table == NULL);
    route->table = table;

    ospf6_route_lock (route);
    table->count++;
    ospf6_route_table_assert (table);

    SET_FLAG (route->flag, OSPF6_ROUTE_ADD);
    if (table->hook_add)
      (*table->hook_add) (route);

    return route;
  }
 
  /* Else, this is the brand new route regarding to the prefix */
  assert (node->info == NULL);
  node->info = route;
  SET_FLAG (route->flag, OSPF6_ROUTE_BEST);
  ospf6_route_lock (route);
  route->installed = now;
  route->changed = now;
  assert (route->table == NULL);
  route->table = table;

  /* lookup real existing next route */
  nextnode = node;
  route_lock_node (nextnode);
  do {
    nextnode = route_next (nextnode);
  } while (nextnode && nextnode->info == NULL);

  /* set next link */
  if (nextnode == NULL)
    route->next = NULL;
  else
  {
    route_unlock_node (nextnode);
                        
    next = nextnode->info;
    route->next = next;
    next->prev = route;
  } 
      
  /* lookup real existing prev route */
  prevnode = node;
  route_lock_node (prevnode);
  do {
    prevnode = route_prev (prevnode);
  } while (prevnode && prevnode->info == NULL);
          
  /* set prev link */
  if (prevnode == NULL)
    route->prev = NULL;
  else
  {
    route_unlock_node (prevnode);
                              
    prev = prevnode->info;
    while (prev->next && ospf6_route_is_same (prev, prev->next))
      prev = prev->next; 
    route->prev = prev;
    prev->next = route;
  } 

  table->count++;
  ospf6_route_table_assert (table);

  SET_FLAG (route->flag, OSPF6_ROUTE_ADD);
  if (table->hook_add)
    (*table->hook_add) (route);

  return route;
}

void ospf6_route_remove(struct ospf6_route * route, struct ospf6_route_table * table)
{
  struct route_node * node;
  struct ospf6_route *current;

  node = route_node_lookup(table->table, &route->prefix);
  assert(node);

  /* find the route to remove, making sure that the route pointer
     is from the route table. */
  current = node->info;
  while(current && ospf6_route_is_same(current, route))
  {
    if(current == route)
      break;
    current = current->next;
  }
  assert(current == route);

  /* adjust doubly linked list */
  if(route->prev)
    route->prev->next = route->next;
  if(route->next)
    route->next->prev = route->prev;

  if(node->info == route)
  {
    if(route->next && ospf6_route_is_same(route->next, route))
    {
      node->info = route->next;
      SET_FLAG(route->next->flag, OSPF6_ROUTE_BEST);
    }
    else
      node->info = NULL;
  }

  table->count--;
  ospf6_route_table_assert(table);

  SET_FLAG(route->flag, OSPF6_ROUTE_WAS_REMOVED);

  if(table->hook_remove)
    (*table->hook_remove) (route);

  ospf6_route_unlock(route);
}

struct ospf6_route * ospf6_route_head(struct ospf6_route_table * table)
{
  struct route_node * node;
  struct ospf6_route * route;

  node = route_top(table->table);
  if(node == NULL)
    return NULL;

  /* skip to the real existing entry */
  while(node && node->info == NULL)
    node = route_next(node);
  if(node == NULL)
    return NULL;

  route_unlock_node(node);
  assert(node->info);

  route = (struct ospf6_route *)node->info;
  assert(route->prev == NULL);
  assert(route->table == table);
  ospf6_route_lock(route);

  return route;
}

struct ospf6_route * ospf6_route_next(struct ospf6_route * route)
{
  struct ospf6_route * next = route->next;

  ospf6_route_unlock(route);
  if(next)
    ospf6_route_lock(next);

  return next;
}

struct ospf6_route *
ospf6_route_best_next (struct ospf6_route *route)
{
  struct route_node *rnode;
  struct ospf6_route *next;

  rnode = route->rnode;
  route_lock_node (rnode);
  rnode = route_next (rnode);
  while (rnode && rnode->info == NULL)
    rnode = route_next (rnode);
  if (rnode == NULL)
    return NULL;
  route_unlock_node (rnode);

  assert (rnode->info);
  next = (struct ospf6_route *) rnode->info;
  ospf6_route_unlock (route);
  ospf6_route_lock (next);
  return next;
}

void ospf6_route_remove_all(struct ospf6_route_table * table)
{
  struct ospf6_route * route;
  for(route = ospf6_route_head(table); route; route = ospf6_route_next(route))
  {
    ospf6_route_remove(route, table);
  }
}

extern void ospf6_route_table_delete (struct ospf6_route_table *table)
{
  ospf6_route_remove_all (table);
  route_table_finish (table->table);
  free(table);
}

struct ospf6_route_table * ospf6_route_table_create(int s, int t)
{
  struct ospf6_route_table * new;
  new = calloc(1, sizeof(struct ospf6_route_table));
  new->table = route_table_init();
  new->scope_type = s;
  new->table_type = t;
  return new;
}
