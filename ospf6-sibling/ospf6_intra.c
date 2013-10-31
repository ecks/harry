#include "config.h"

#include "stdlib.h"
#include "stdbool.h"
#include "stdint.h"
#include "string.h"
#include "assert.h"
#include "netinet/in.h"

#include "dblist.h"
#include "thread.h"
#include "routeflow-common.h"
#include "prefix.h"
#include "if.h"
#include "ospf6_lsa.h"
#include "ospf6_lsdb.h"
#include "ospf6_proto.h"
#include "ospf6_top.h"
#include "ospf6_route.h"
#include "ospf6_area.h"
#include "ospf6_interface.h"
#include "ospf6_intra.h"

int ospf6_link_lsa_originate(struct thread * thread)
{
  char buffer[OSPF6_MAX_LSASIZE];
  struct ospf6_lsa_header *lsa_header;

  struct ospf6_lsa * old,* lsa;
  struct ospf6_link_lsa *link_lsa;
  struct ospf6_interface * oi;
  struct ospf6_prefix * op;

  oi = (struct ospf6_interface *)THREAD_ARG(thread);
  oi->thread_link_lsa = NULL;

  assert(oi->area);

  /* find previous LSA */
  old = ospf6_lsdb_lookup (htons (OSPF6_LSTYPE_LINK),
                           htonl (oi->interface->ifindex),
                           oi->area->ospf6->router_id, oi->lsdb);

  /* can't make Link-LSA if linklocal address not set */
  if (oi->linklocal_addr == NULL)
  {    
//    if (IS_OSPF6_DEBUG_ORIGINATE (LINK))
//      zlog_debug ("No Linklocal address on %s, defer originating",
//                                                   oi->interface->name);
    if (old)
      ospf6_lsa_purge (old);
    return 0;
  }

  /* prepare buffer */
  memset (buffer, 0, sizeof (buffer));
  lsa_header = (struct ospf6_lsa_header *) buffer;
  link_lsa = (struct ospf6_link_lsa *)
              ((void *) lsa_header + sizeof (struct ospf6_lsa_header));

  /* Fill Link-LSA */
  link_lsa->priority = oi->priority;
  memcpy (link_lsa->options, oi->area->options, 3);
  memcpy (&link_lsa->linklocal_addr, oi->linklocal_addr,
          sizeof (struct in6_addr));
  link_lsa->prefix_num = htonl (oi->route_connected->count);

  op = (struct ospf6_prefix *)
        ((void *) link_lsa + sizeof (struct ospf6_link_lsa));

  /* connected prefix to advertise */
  struct ospf6_route * route;
  for (route = ospf6_route_head (oi->route_connected); route;
       route = ospf6_route_next (route))
  {
    op->prefix_length = route->prefix.prefixlen;
    op->prefix_options = route->path.prefix_options;
    op->prefix_metric = htons (0); 
    memcpy (OSPF6_PREFIX_BODY (op), &route->prefix.u.prefix6,
            OSPF6_PREFIX_SPACE (op->prefix_length));
    op = OSPF6_PREFIX_NEXT (op);
  }    

  /* Fill LSA Header */
  lsa_header->age = 0;
  lsa_header->type = htons(OSPF6_LSTYPE_LINK);
  lsa_header->id = htonl(oi->interface->ifindex);
  lsa_header->adv_router = oi->area->ospf6->router_id;
  lsa_header->seqnum =
    ospf6_new_ls_seqnum(lsa_header->type, lsa_header->id,
                        lsa_header->adv_router, oi->lsdb);
  lsa_header->length = htons((void *)op - (void *)buffer);
 
  /* LSA checksum */
  ospf6_lsa_checksum (lsa_header);

  /* create LSA */
  lsa = ospf6_lsa_create (lsa_header);

  /* Originate */
  ospf6_lsa_originate_interface (lsa, oi);

  return 0;
}
