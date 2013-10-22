#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include <netinet/in.h>

#include "util.h"
#include "routeflow-common.h"
#include "dblist.h"
#include "prefix.h"
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
  list_init(&ifp->connected); 
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

/* Interface existence check by ifindex. */
struct interface * if_lookup_by_index(const unsigned int index)
{
  struct interface * ifp, * next;

  LIST_FOR_EACH_SAFE(ifp, next, struct interface, node, iflist)
  {
    if(index == ifp->ifindex)
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

int if_is_pointtopoint(struct interface * ifp)
{
  return 0;  // disable p2p for now
}

/* Allocate connected structure. */
struct connected * connected_new(void)
{
  return calloc(1, sizeof(struct connected));
}

/* Free connected structure. */
void connected_free(struct connected * connected)
{
  if(connected->address)
    prefix_free(connected->address);

  if(connected->destination)
    prefix_free(connected->destination);

  free(connected);
}

struct connected * connected_add_by_prefix(struct interface * ifp, struct prefix * p, struct prefix * destination)
{
  struct connected * ifc;

  /* Allocate new connected address. */
  ifc = connected_new();
  ifc->ifp = ifp;

  /* Fetch interface address */
  ifc->address = prefix_new();
  memcpy(ifc->address, p, sizeof(struct prefix));

  /* Fetch dest address */
  if(destination)
  {
    ifc->destination = prefix_new();
    memcpy(ifc->destination, destination, sizeof(struct prefix));
  }

  list_push_back(&ifp->connected, &ifc->node);
}
