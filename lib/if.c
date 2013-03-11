#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>

#include "util.h"
#include "routeflow-common.h"
#include "dblist.h"
#include "if.h"

/* Master list of interfaces. */
struct list * iflist;

void if_init()
{
  iflist = list_new();
  list_init(iflist);
}

/* Create new interface structure */
struct interface * if_create(const char * name, int namelen)
{
  struct interface * ifp;

  ifp = calloc(1, sizeof(struct interface));

  ifp->ifindex = IFINDEX_INTERNAL;

  list_init(&ifp->node);

  assert(name);
  assert (namelen <= RFP_MAX_PORT_NAME_LEN); /* Need space for '\0' at end. */
  strncpy (ifp->name, name, namelen);
  ifp->name[namelen] = '\0';
  if (if_lookup_by_name(ifp->name) == NULL)
    list_push_back(iflist, &ifp->node);
//    listnode_add_sort (iflist, ifp);
  else
    printf("if_create(%s): corruption detected -- interface with this "
             "name exists already!", ifp->name);
  ifp->connected = list_new (); 
//  ifp->connected->del = (void (*) (void *)) connected_free;

  return ifp;
}

/* Interface existance check by interface name. */
struct interface *
if_lookup_by_name (const char *name)
{
  struct interface * ifp, * next;

  if (name)
    LIST_FOR_EACH_SAFE(ifp, next, struct interface, node, iflist)
    {
        if (strcmp(name, ifp->name) == 0)
          return ifp;
    }
  return NULL;
}

/* Get interface by name if given name interface doesn't exist create
   one. */
struct interface *
if_get_by_name (const char *name)
{
  struct interface *ifp;

  return ((ifp = if_lookup_by_name(name)) != NULL) ? ifp :
         if_create(name, strlen(name));
}

/* Is interface up? */
int if_is_up(struct interface * ifp)
{
  return ifp->state == RFPPS_FORWARD;
}
