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
  return (struct ospf6_lsa *)node->info;
}

void
ospf6_lsdb_add (struct ospf6_lsa *lsa, struct ospf6_lsdb *lsdb)
{
  struct prefix_ipv6 key;
  struct route_node *current, *nextnode, *prevnode;
  struct ospf6_lsa *next, *prev, *old = NULL;

  memset (&key, 0, sizeof (key));
  ospf6_lsdb_set_key (&key, &lsa->header->type, sizeof (lsa->header->type));
  ospf6_lsdb_set_key (&key, &lsa->header->adv_router,
                      sizeof (lsa->header->adv_router));
  ospf6_lsdb_set_key (&key, &lsa->header->id, sizeof (lsa->header->id));

  current = route_node_get (lsdb->table, (struct prefix *) &key);
  old = current->info;
  current->info = lsa;
  ospf6_lsa_lock (lsa);

  if (old)
  {   
    if (old->prev)
      old->prev->next = lsa;
    if (old->next)
      old->next->prev = lsa;
    lsa->next = old->next;
    lsa->prev = old->prev;
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
      lsa->next = NULL;
    else
    {   
      next = nextnode->info;
      lsa->next = next;
      next->prev = lsa;
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
      lsa->prev = NULL;
    else
    {
      prev = prevnode->info;
      lsa->prev = prev;
      prev->next = lsa;
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
          (*lsdb->hook_remove) (old);
          (*lsdb->hook_remove) (lsa);
        }
      }
      else if (OSPF6_LSA_IS_MAXAGE (old))
      {
        if (lsdb->hook_add)
          (*lsdb->hook_add) (lsa);
      }
      else
      {
        if (lsdb->hook_remove)
          (*lsdb->hook_remove) (old);
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

  ospf6_lsdb_set_key (&key, &lsa->header->type, sizeof (lsa->header->type));
  ospf6_lsdb_set_key (&key, &lsa->header->adv_router,
                                sizeof (lsa->header->adv_router));
  ospf6_lsdb_set_key (&key, &lsa->header->id, sizeof (lsa->header->id));

  node = route_node_lookup (lsdb->table, (struct prefix *) &key);
  assert (node && node->info == lsa);

  if (lsa->prev)
    lsa->prev->next = lsa->next;
  if (lsa->next)
    lsa->next->prev = lsa->prev;

  node->info = NULL;
  lsdb->count--;

  if(lsdb->hook_remove)
    (*lsdb->hook_remove) (lsa);

  ospf6_lsa_unlock(lsa);
  route_unlock_node(node);

//  ospf6_lsdb_count_assert(lsdb);
}

/* Interation function */
struct ospf6_lsa * ospf6_lsdb_head(struct ospf6_lsdb * lsdb)
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
    ospf6_lsa_lock ((struct ospf6_lsa *) node->info);
  return (struct ospf6_lsa *) node->info;
}

struct ospf6_lsa * ospf6_lsdb_next(struct ospf6_lsa * lsa)
{
  struct ospf6_lsa *next = lsa->next;

  ospf6_lsa_unlock (lsa);
  if (next)
    ospf6_lsa_lock (next);

  return next;
} 

struct ospf6_lsa *
ospf6_lsdb_type_router_head (u_int16_t type, u_int32_t adv_router,
                                 struct ospf6_lsdb *lsdb)
{
  struct route_node *node;
  struct prefix_ipv6 key;
  struct ospf6_lsa *lsa;

  memset (&key, 0, sizeof (key));
  ospf6_lsdb_set_key (&key, &type, sizeof (type));
  ospf6_lsdb_set_key (&key, &adv_router, sizeof (adv_router));

  node = lsdb->table->top;

  /* Walk down tree. */
  while (node && node->p.prefixlen <= key.prefixlen &&
         prefix_match (&node->p, (struct prefix *) &key))
         node = node->link[prefix6_bit(&key.prefix, node->p.prefixlen)];

  if (node)
    route_lock_node (node);
  while (node && node->info == NULL)
    node = route_next (node);

  if (node == NULL)
    return NULL;
  else
    route_unlock_node (node);

  if (! prefix_match ((struct prefix *) &key, &node->p))
    return NULL;

  lsa = node->info;
  ospf6_lsa_lock (lsa);

  return lsa;
}

struct ospf6_lsa *
ospf6_lsdb_type_router_next (u_int16_t type, u_int32_t adv_router,
                                 struct ospf6_lsa *lsa)
{
  struct ospf6_lsa *next = lsa->next;

  if (next)
  {   
    if (next->header->type != type ||
      next->header->adv_router != adv_router)
    next = NULL;
  }   

  if (next)
    ospf6_lsa_lock (next);
  ospf6_lsa_unlock (lsa);
  return next;
}

void ospf6_lsdb_remove_all(struct ospf6_lsdb * lsdb)
{
  struct ospf6_lsa * lsa;
  for(lsa = ospf6_lsdb_head(lsdb); lsa; lsa = ospf6_lsdb_next(lsa))
    ospf6_lsdb_remove(lsa, lsdb);
}

/* Decide new LS sequence number to originate.
   note return value is network byte order */
u_int32_t
ospf6_new_ls_seqnum (u_int16_t type, u_int32_t id, u_int32_t adv_router,
                         struct ospf6_lsdb *lsdb)
{
  struct ospf6_lsa *lsa;
  signed long seqnum = 0;

  /* if current database copy not found, return InitialSequenceNumber */
  lsa = ospf6_lsdb_lookup (type, id, adv_router, lsdb);
  if (lsa == NULL)
    seqnum = INITIAL_SEQUENCE_NUMBER;
  else
    seqnum = (signed long) ntohl (lsa->header->seqnum) + 1;

  return ((u_int32_t) htonl (seqnum));
}
