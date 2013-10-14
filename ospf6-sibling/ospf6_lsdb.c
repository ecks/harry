#include "config.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <netinet/in.h>

#include "util.h"
#include "dblist.h"
#include "prefix.h"
#include "prefix.h"
#include "table.h"
#include "ospf6_proto.h"
#include "ospf6_lsa.h"
#include "ospf6_lsdb.h"

struct ospf6_lsdb * ospf6_lsdb_create(void * data)
{
  struct ospf6_lsdb * lsdb;

  if((lsdb = calloc(1, sizeof(struct ospf6_lsdb))) == NULL)
  {
    zlog_warn("Can't malloc lsdb");
    return NULL; 
  }
 
  memset(lsdb, 0, sizeof(struct ospf6_lsdb));

  lsdb->data = data;
  lsdb->table = route_table_init();
  return lsdb;
}

static void ospf6_lsdb_set_key(struct prefix_ipv6 * key, void * value, int len)
{
  assert (key->prefixlen % 8 == 0); 

  memcpy ((caddr_t) &key->prefix + key->prefixlen / 8,
          (caddr_t) value, len);
  key->family = AF_INET6;
  key->prefixlen += len * 8;

}

struct ospf6_lsa * ospf6_lsdb_lookup(u_int16_t type, u_int32_t id, u_int32_t adv_router, struct ospf6_lsdb * lsdb)
{
  struct route_node * node;
  struct prefix_ipv6 key;

  if(lsdb == NULL)
    return NULL;

  memset(&key, 0, sizeof(key));
  ospf6_lsdb_set_key(&key, &type, sizeof(type));
  ospf6_lsdb_set_key(&key, &adv_router, sizeof(adv_router));
  ospf6_lsdb_set_key(&key, &id, sizeof(id));

  node = route_node_lookup(lsdb->table, (struct prefix *)&key);
  if(node == NULL || node->info == NULL)
    return NULL;
  return (struct ospf6_lsa *)CONTAINER_OF(node->info, struct ospf6_lsa, node);
}

void
ospf6_lsdb_add (struct ospf6_lsa *lsa, struct ospf6_lsdb *lsdb)
{
  struct prefix_ipv6 key;
  struct route_node *current, *nextnode, *prevnode;
  struct list *next, *prev, *old = NULL;

  memset (&key, 0, sizeof (key));
  ospf6_lsdb_set_key (&key, &lsa->header->type, sizeof (lsa->header->type));
  ospf6_lsdb_set_key (&key, &lsa->header->adv_router,
                      sizeof (lsa->header->adv_router));
  ospf6_lsdb_set_key (&key, &lsa->header->id, sizeof (lsa->header->id));

  current = route_node_get (lsdb->table, (struct prefix *) &key);
  old = current->info;
  current->info = &lsa->node;
  ospf6_lsa_lock (lsa);

  if (old)
  {   
    if (old->prev)
      old->prev->next = &lsa->node;
    if (old->next)
      old->next->prev = &lsa->node;
    lsa->node.next = old->next;
    lsa->node.prev = old->prev;
  }   
  else
  {   
    /* next link */
    nextnode = current;
    route_lock_node (nextnode);
    do 
    {
      nextnode = route_next (nextnode);
    } while (nextnode && nextnode->info == NULL);
    if (nextnode == NULL)
      lsa->node.next = NULL;
    else
    {   
      next = nextnode->info;
      lsa->node.next = next;
      next->prev = &lsa->node;
      route_unlock_node (nextnode);
    }

    /* prev link */
    prevnode = current;
    route_lock_node (prevnode);
    do 
    {
      prevnode = route_prev (prevnode);
    } while (prevnode && prevnode->info == NULL);
    if (prevnode == NULL)
      lsa->node.prev = NULL;
    else
    {
      prev = prevnode->info;
      lsa->node.prev = prev;
      prev->next = &lsa->node;
      route_unlock_node (prevnode);
    }

    lsdb->count++;
  }

  if (old)
  {
    if (OSPF6_LSA_IS_CHANGED (old, lsa))
    {
      if (OSPF6_LSA_IS_MAXAGE (lsa))
      {
        if (lsdb->hook_remove)
        {
          (*lsdb->hook_remove) (CONTAINER_OF(old, struct ospf6_lsa, node));
          (*lsdb->hook_remove) (lsa);
        }
      }
      else if (OSPF6_LSA_IS_MAXAGE (CONTAINER_OF(old, struct ospf6_lsa, node)))
      {
        if (lsdb->hook_add)
          (*lsdb->hook_add) (lsa);
      }
      else
      {
        if (lsdb->hook_remove)
          (*lsdb->hook_remove) (CONTAINER_OF(old, struct ospf6_lsa, node));
        if (lsdb->hook_add)
          (*lsdb->hook_add) (lsa);
      }
    }
  }
  else if (OSPF6_LSA_IS_MAXAGE (lsa))
  {
    if (lsdb->hook_remove)
      (*lsdb->hook_remove) (lsa);
  }
  else
  {
    if (lsdb->hook_add)
      (*lsdb->hook_add) (lsa);
  }

  if (old)
    ospf6_lsa_unlock (old);

//  ospf6_lsdb_count_assert (lsdb); (implement later)

}

void ospf6_lsdb_remove(struct ospf6_lsa * lsa, struct ospf6_lsdb * lsdb)
{
  struct route_node * node;
  struct prefix_ipv6 key;

  memset(&key, 0, sizeof(key));

  // to be continued
}

/* Interation function */
struct list * ospf6_lsdb_head(struct ospf6_lsdb * lsdb)
{
  struct route_node *node;

  node = route_top (lsdb->table);
  if (node == NULL)
    return NULL;

  /* skip to the existing lsdb entry */
  while (node && node->info == NULL)
         node = route_next (node);
  if (node == NULL)
    return NULL;

  route_unlock_node (node);
  if (node->info)
    ospf6_lsa_lock ((struct ospf6_lsa *) CONTAINER_OF(node->info, struct ospf6_lsa, node));
  return (struct list *) node->info;
}

/*struct ospf6_lsa * ospf6_lsdb_next(struct ospf6_lsa * lsa)
{
  struct ospf6_lsa *next = lsa->next;

  ospf6_lsa_unlock (lsa);
  if (next)
    ospf6_lsa_lock (next);

  return next;
} */

void ospf6_lsdb_remove_all(struct ospf6_lsdb * lsdb)
{
  struct ospf6_lsa * lsa;
  struct list * lsa_list = ospf6_lsdb_head(lsdb);

  // check that list is initialized
  if(lsa_list)
  {
    LIST_FOR_EACH(lsa, struct ospf6_lsa, node, ospf6_lsdb_head(lsdb))
    {
      ospf6_lsdb_remove(lsa, lsdb);
    }
  }
}
