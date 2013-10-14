#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "dblist.h"
#include "prefix.h"
#include "table.h"

void route_node_delete (struct route_node *);
void route_table_free (struct route_table *);

struct route_table *
route_table_init (void)
{
  struct route_table *rt;

  rt = calloc (1, sizeof (struct route_table));
  return rt;
}

void
route_table_finish (struct route_table *rt)
{
  route_table_free (rt);
}

/* Allocate new route node. */
static struct route_node *
route_node_new (void)
{
  struct route_node *node;
  node = calloc (1, sizeof (struct route_node));
  return node;
}

struct route_node * route_prev (struct route_node *node)
{
  struct route_node *end;
  struct route_node *prev = NULL;

  end = node;
  node = node->parent;
  if (node)
    route_lock_node (node);
  while (node)
  {    
    prev = node;
    node = route_next (node);
    if (node == end) 
    {    
      route_unlock_node (node);
      node = NULL;
    }    
  }    
  route_unlock_node (end);
  if (prev)
    route_lock_node (prev);

  return prev;
}

/* Allocate new route node with prefix set. */
static struct route_node *
route_node_set (struct route_table *table, struct prefix *prefix)
{
  struct route_node *node;
  
  node = route_node_new ();

  prefix_copy (&node->p, prefix);
  node->table = table;

  return node;
}

/* Free route node. */
static void
route_node_free (struct route_node *node)
{
  free (node);
}

/* Free route table. */
void
route_table_free (struct route_table *rt)
{
  struct route_node *tmp_node;
  struct route_node *node;
 
  if (rt == NULL)
    return;

  node = rt->top;

  while (node)
    {
      if (node->l_left)
	{
	  node = node->l_left;
	  continue;
	}

      if (node->l_right)
	{
	  node = node->l_right;
	  continue;
	}

      tmp_node = node;
      node = node->parent;

      if (node != NULL)
	{
	  if (node->l_left == tmp_node)
	    node->l_left = NULL;
	  else
	    node->l_right = NULL;

	  route_node_free (tmp_node);
	}
      else
	{
	  route_node_free (tmp_node);
	  break;
	}
    }
 
  free (rt);
  return;
}

void
route_table_show(struct route_table * rt)
{
  struct route_node * rn;

  for(rn = route_top(rt); rn; rn = route_next(rn))
  {
    if(rn->info != NULL)
      printf("Not null: %s/%d\n", inet_ntoa(rn->p.u.prefix4), rn->p.prefixlen);
    else
      printf("Null: %s/%d\n", inet_ntoa(rn->p.u.prefix4), rn->p.prefixlen);
  }
}

void
route_node_show(struct route_node * rn)
{
  if(rn->info != NULL)
    printf("Not null: %s/%d\n", inet_ntoa(rn->p.u.prefix4), rn->p.prefixlen);
  else
    printf("Null: %s/%d\n", inet_ntoa(rn->p.u.prefix4), rn->p.prefixlen);
}

/* Utility mask array. */
static const u_char maskbit[] =
{
  0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff
};

/* Common prefix route genaration. */
static void
route_common (struct prefix *n, struct prefix *p, struct prefix *new)
{
  int i;
  u_char diff;
  u_char mask;

  u_char *np = (u_char *)&n->u.prefix;
  u_char *pp = (u_char *)&p->u.prefix;
  u_char *newp = (u_char *)&new->u.prefix;

  for (i = 0; i < p->prefixlen / 8; i++)
    {
      if (np[i] == pp[i])
	newp[i] = np[i];
      else
	break;
    }

  new->prefixlen = i * 8;

  if (new->prefixlen != p->prefixlen)
    {
      diff = np[i] ^ pp[i];
      mask = 0x80;
      while (new->prefixlen < p->prefixlen && !(mask & diff))
	{
	  mask >>= 1;
	  new->prefixlen++;
	}
      newp[i] = np[i] & maskbit[new->prefixlen % 8];
    }
}

static void
set_link (struct route_node *node, struct route_node *new)
{
  unsigned int bit = prefix_bit (&new->p.u.prefix, node->p.prefixlen);

  node->link[bit] = new;
  new->parent = node;
}

/* Lock node. */
struct route_node *
route_lock_node (struct route_node *node)
{
  node->lock++;
  return node;
}

/* Unlock node. */
void
route_unlock_node (struct route_node *node)
{
  node->lock--;

  if (node->lock == 0)
    route_node_delete (node);
}

/* Find matched prefix. */
struct route_node *
route_node_match (const struct route_table *table, const struct prefix *p)
{
  struct route_node *node;
  struct route_node *matched;

  matched = NULL;
  node = table->top;

  /* Walk down tree.  If there is matched route then store it to
     matched. */
  while (node && node->p.prefixlen <= p->prefixlen && 
	 prefix_match (&node->p, p))
    {
      if (node->info)
	matched = node;
      node = node->link[prefix_bit(&p->u.prefix, node->p.prefixlen)];
    }

  /* If matched route found, return it. */
  if (matched)
    return route_lock_node (matched);

  return NULL;
}

struct route_node *
route_node_match_ipv4 (const struct route_table *table,
		       const struct in_addr *addr)
{
  struct prefix_ipv4 p;

  memset (&p, 0, sizeof (struct prefix_ipv4));
  p.family = AF_INET;
  p.prefixlen = IPV4_MAX_PREFIXLEN;
  p.prefix = *addr;

  return route_node_match (table, (struct prefix *) &p);
}

#ifdef HAVE_IPV6
struct route_node *
route_node_match_ipv6 (const struct route_table *table,
		       const struct in6_addr *addr)
{
  struct prefix_ipv6 p;

  memset (&p, 0, sizeof (struct prefix_ipv6));
  p.family = AF_INET6;
  p.prefixlen = IPV6_MAX_PREFIXLEN;
  p.prefix = *addr;

  return route_node_match (table, (struct prefix *) &p);
}
#endif /* HAVE_IPV6 */

/* Lookup same prefix node.  Return NULL when we can't find route. */
struct route_node *
route_node_lookup (struct route_table *table, struct prefix *p)
{
  struct route_node *node;

  node = table->top;

  while (node && node->p.prefixlen <= p->prefixlen && 
	 prefix_match (&node->p, p))
    {
      if (node->p.prefixlen == p->prefixlen && node->info)
	return route_lock_node (node);

      node = node->link[prefix_bit(&p->u.prefix, node->p.prefixlen)];
    }

  return NULL;
}

/* Add node to routing table. */
struct route_node *
route_node_get (struct route_table *table, struct prefix *p)
{
  struct route_node *new;
  struct route_node *node;
  struct route_node *match;

  match = NULL;
  node = table->top;
  while (node && node->p.prefixlen <= p->prefixlen && 
	 prefix_match (&node->p, p))
    {
      if (node->p.prefixlen == p->prefixlen)
	{
	  route_lock_node (node);
	  return node;
	}
      match = node;
      node = node->link[prefix_bit(&p->u.prefix, node->p.prefixlen)];
    }

  if (node == NULL)
    {
      new = route_node_set (table, p);
      if (match)
	set_link (match, new);
      else
	table->top = new;
    }
  else
    {
      new = route_node_new ();
      route_common (&node->p, p, &new->p);
      new->p.family = p->family;
      new->table = table;
      set_link (new, node);

      if (match)
	set_link (match, new);
      else
      {
	table->top = new;
      }

      if (new->p.prefixlen != p->prefixlen)
	{
	  match = new;
	  new = route_node_set (table, p);
	  set_link (match, new);
	}
    }
  route_lock_node (new);
  
  return new;
}

/* Delete node from the routing table. */
void
route_node_delete (struct route_node *node)
{
  struct route_node *child;
  struct route_node *parent;

  assert (node->lock == 0);
  assert (node->info == NULL);

  if (node->l_left && node->l_right)
    return;

  if (node->l_left)
    child = node->l_left;
  else
    child = node->l_right;

  parent = node->parent;

  if (child)
    child->parent = parent;

  if (parent)
    {
      if (parent->l_left == node)
	parent->l_left = child;
      else
	parent->l_right = child;
    }
  else
    node->table->top = child;

  route_node_free (node);

  /* If parent node is stub then delete it also. */
  if (parent && parent->lock == 0)
    route_node_delete (parent);
}

/* Get fist node and lock it.  This function is useful when one want
   to lookup all the node exist in the routing table. */
struct route_node *
route_top (struct route_table *table)
{
  /* If there is no node in the routing table return NULL. */
  if (table->top == NULL)
    return NULL;

  /* Lock the top node and return it. */
  route_lock_node (table->top);
  return table->top;
}

/* Unlock current node and lock next node then return it. */
struct route_node *
route_next (struct route_node *node)
{
  struct route_node *next;
  struct route_node *start;

  /* Node may be deleted from route_unlock_node so we have to preserve
     next node's pointer. */

  if (node->l_left)
    {
      next = node->l_left;
      route_lock_node (next);
      route_unlock_node (node);
      return next;
    }
  if (node->l_right)
    {
      next = node->l_right;
      route_lock_node (next);
      route_unlock_node (node);
      return next;
    }

  start = node;
  while (node->parent)
    {
      if (node->parent->l_left == node && node->parent->l_right)
	{
	  next = node->parent->l_right;
	  route_lock_node (next);
	  route_unlock_node (start);
	  return next;
	}
      node = node->parent;
    }
  route_unlock_node (start);
  return NULL;
}

/* Unlock current node and lock next node until limit. */
struct route_node *
route_next_until (struct route_node *node, struct route_node *limit)
{
  struct route_node *next;
  struct route_node *start;

  /* Node may be deleted from route_unlock_node so we have to preserve
     next node's pointer. */

  if (node->l_left)
    {
      next = node->l_left;
      route_lock_node (next);
      route_unlock_node (node);
      return next;
    }
  if (node->l_right)
    {
      next = node->l_right;
      route_lock_node (next);
      route_unlock_node (node);
      return next;
    }

  start = node;
  while (node->parent && node != limit)
    {
      if (node->parent->l_left == node && node->parent->l_right)
	{
	  next = node->parent->l_right;
	  route_lock_node (next);
	  route_unlock_node (start);
	  return next;
	}
      node = node->parent;
    }
  route_unlock_node (start);
  return NULL;
}
