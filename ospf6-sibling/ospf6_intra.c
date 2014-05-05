#include "config.h"

#include "stdlib.h"
#include "stdbool.h"
#include "stdint.h"
#include "string.h"
#include "assert.h"
#include "netinet/in.h"

#include "util.h"
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
#include "ospf6_neighbor.h"
#include "ospf6_intra.h"

int ospf6_router_lsa_originate (struct thread *thread)
{
  char buffer[OSPF6_MAX_LSASIZE];
  struct ospf6_lsa_header * lsa_header;

  struct ospf6_router_lsa * router_lsa;
  struct ospf6_router_lsdesc *lsdesc;
  u_int16_t type;
  u_int32_t router;
  int count;
  struct ospf6_lsa * lsa;
  struct ospf6_area * oa;
  u_int32_t link_state_id = 0;
  struct ospf6_interface * oi;
  struct ospf6_neighbor * on, * drouter = NULL;

  oa = (struct ospf6_area *) THREAD_ARG(thread);
  oa->thread_router_lsa = NULL;

  memset(buffer, 0, sizeof(buffer));
  lsa_header = (struct ospf6_lsa_header *)buffer;
  router_lsa = (struct ospf6_router_lsa *)((void *) lsa_header + sizeof (struct ospf6_lsa_header));

  OSPF6_OPT_SET (router_lsa->options, OSPF6_OPT_V6);
  OSPF6_OPT_SET (router_lsa->options, OSPF6_OPT_E);
  OSPF6_OPT_CLEAR (router_lsa->options, OSPF6_OPT_MC);
  OSPF6_OPT_CLEAR (router_lsa->options, OSPF6_OPT_N);
  OSPF6_OPT_SET (router_lsa->options, OSPF6_OPT_R);
  OSPF6_OPT_CLEAR (router_lsa->options, OSPF6_OPT_DC);

  if (ospf6_is_router_abr (ospf6))
    SET_FLAG (router_lsa->bits, OSPF6_ROUTER_BIT_B);
  else
    UNSET_FLAG (router_lsa->bits, OSPF6_ROUTER_BIT_B);
  if (ospf6_asbr_is_asbr (ospf6))
    SET_FLAG (router_lsa->bits, OSPF6_ROUTER_BIT_E);
  else
    UNSET_FLAG (router_lsa->bits, OSPF6_ROUTER_BIT_E);
  UNSET_FLAG (router_lsa->bits, OSPF6_ROUTER_BIT_V);
  UNSET_FLAG (router_lsa->bits, OSPF6_ROUTER_BIT_W);

  /* describe links for each interfaces */
  lsdesc = (struct ospf6_router_lsdesc *)((void *) router_lsa + sizeof (struct ospf6_router_lsa));

  LIST_FOR_EACH(oi, struct ospf6_interface, node, &oa->if_list)
  {
    /* Interfaces in state Down or Loopback are not described */
    if (oi->state == OSPF6_INTERFACE_DOWN ||
        oi->state == OSPF6_INTERFACE_LOOPBACK)
      continue;

    /* Nor are interfaces without any full adjacencies described */
    count = 0;
    LIST_FOR_EACH(on, struct ospf6_neighbor, node, &oi->neighbor_list)
      if (on->state == OSPF6_NEIGHBOR_FULL)
        count++;
                       
    if (count == 0)
      continue;

    /* Multiple Router-LSA instance according to size limit setting */
    if((oa->router_lsa_size_limit != 0)
       && ((void *) lsdesc + sizeof (struct ospf6_router_lsdesc) -
      /* XXX warning: comparison between signed and unsigned */
       (void *) buffer > oa->router_lsa_size_limit))
    {
      if ((caddr_t) lsdesc == (caddr_t) router_lsa +
          sizeof (struct ospf6_router_lsa))
      {
//        if (IS_OSPF6_DEBUG_ORIGINATE (ROUTER))
//          zlog_debug ("Size limit setting for Router-LSA too short");
        return 0;
      }

      /* Fill LSA Header */
      lsa_header->age = 0;
      lsa_header->type = htons (OSPF6_LSTYPE_ROUTER);
      lsa_header->id = htonl (link_state_id);
      lsa_header->adv_router = oa->ospf6->router_id;
      lsa_header->seqnum =
      ospf6_new_ls_seqnum (lsa_header->type, lsa_header->id,
                           lsa_header->adv_router, oa->lsdb);
      lsa_header->length = htons ((caddr_t) lsdesc - (caddr_t) buffer);

      /* LSA checksum */
      ospf6_lsa_checksum (lsa_header);

      /* create LSA */
      lsa = ospf6_lsa_create (lsa_header);

      /* Originate */
      ospf6_lsa_originate_area (lsa, oa);

      /* Reset setting for consecutive origination */
      memset ((caddr_t) router_lsa + sizeof (struct ospf6_router_lsa),
              0, (caddr_t) lsdesc - (caddr_t) router_lsa);
      lsdesc = (struct ospf6_router_lsdesc *)((caddr_t) router_lsa + sizeof (struct ospf6_router_lsa));
      link_state_id ++;
    }

    /* Point-to-Point interfaces */
    if (if_is_pointopoint (oi->interface))
    {
      LIST_FOR_EACH(on, struct ospf6_neighbor, node, &oi->neighbor_list)
      {
        if (on->state != OSPF6_NEIGHBOR_FULL)
          continue;

        lsdesc->type = OSPF6_ROUTER_LSDESC_POINTTOPOINT;
        lsdesc->metric = htons (oi->cost);
        lsdesc->interface_id = htonl (oi->interface->ifindex);
        lsdesc->neighbor_interface_id = htonl (on->ifindex);
        lsdesc->neighbor_router_id = on->router_id;

        lsdesc++;
      }
    }

    /* Broadcast and NBMA interfaces */
    if (if_is_broadcast (oi->interface))
    {
      /* If this router is not DR,
       * and If this router not fully adjacent with DR,
       * this interface is not transit yet: ignore. */
      if (oi->state != OSPF6_INTERFACE_DR)
      {
        drouter = ospf6_neighbor_lookup (oi->drouter, oi);
        if (drouter == NULL || drouter->state != OSPF6_NEIGHBOR_FULL)
          continue;
      }

      lsdesc->type = OSPF6_ROUTER_LSDESC_TRANSIT_NETWORK;
      lsdesc->metric = htons (oi->cost);
      lsdesc->interface_id = htonl (oi->interface->ifindex);
      if (oi->state != OSPF6_INTERFACE_DR)
      {
        lsdesc->neighbor_interface_id = htonl (drouter->ifindex);
        lsdesc->neighbor_router_id = drouter->router_id;
      }
      else
      {
        lsdesc->neighbor_interface_id = htonl (oi->interface->ifindex);
        lsdesc->neighbor_router_id = oi->area->ospf6->router_id;
      }

      lsdesc++;
    }

    /* Virtual links */
    /* xxx */
    /* Point-to-Multipoint interfaces */
    /* xxx */


  }

  if ((caddr_t) lsdesc != (caddr_t) router_lsa +
      sizeof (struct ospf6_router_lsa))
  {
    /* Fill LSA Header */
    lsa_header->age = 0;
    lsa_header->type = htons (OSPF6_LSTYPE_ROUTER);
    lsa_header->id = htonl (link_state_id);
    lsa_header->adv_router = oa->ospf6->router_id;
    lsa_header->seqnum =
      ospf6_new_ls_seqnum (lsa_header->type, lsa_header->id, lsa_header->adv_router, oa->lsdb);
    lsa_header->length = htons ((caddr_t) lsdesc - (caddr_t) buffer);

    /* LSA checksum */
    ospf6_lsa_checksum (lsa_header);

    /* create LSA */
    lsa = ospf6_lsa_create (lsa_header);

    /* Originate */
    ospf6_lsa_originate_area (lsa, oa);

    link_state_id ++;
  }
  else
  {
//    if(IS_OSPF6_DEBUG_ORIGINATE (ROUTER))
//    zlog_debug ("Nothing to describe in Router-LSA, suppress");
  }

  /* Do premature-aging of rest, undesired Router-LSAs */
  type = ntohs (OSPF6_LSTYPE_ROUTER);
  router = oa->ospf6->router_id;
  for (lsa = ospf6_lsdb_type_router_head (type, router, oa->lsdb); lsa;
       lsa = ospf6_lsdb_type_router_next (type, router, lsa))
  {
    if (ntohl (lsa->header->id) < link_state_id)
      continue;
    ospf6_lsa_purge (lsa);
  }


  return 0;
}

int ospf6_network_lsa_originate (struct thread *thread)
{
  return 0;
}

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

int ospf6_intra_prefix_lsa_originate_transit(struct thread * thread)
{
  struct ospf6_interface * oi;
  char buffer[OSPF6_MAX_LSASIZE];
  struct ospf6_lsa_header *lsa_header;
  struct ospf6_lsa * old, * lsa;

  struct ospf6_intra_prefix_lsa * intra_prefix_lsa;
  struct ospf6_neighbor * on;
  struct ospf6_route * route;
  struct ospf6_prefix * op;
  int full_count = 0;
  unsigned short prefix_num = 0;
  struct ospf6_route_table * route_advertise;
  struct ospf6_link_lsa * link_lsa;
  char * start, * end, * current;
  u_int16_t type;

  oi = (struct ospf6_interface *) THREAD_ARG (thread);
  oi->thread_intra_prefix_lsa = NULL;

  assert (oi->area);

  /* find previous LSA */
  old = ospf6_lsdb_lookup (htons (OSPF6_LSTYPE_INTRA_PREFIX),
                                  htonl (oi->interface->ifindex),
                                  oi->area->ospf6->router_id, oi->area->lsdb);

//  if(CHECK_FLAG (oi->flag, OSPF6_INTERFACE_DISABLE))
//  {    
//    if (old)
//      ospf6_lsa_purge (old);
//    return 0;
//  }    

//  if (IS_OSPF6_DEBUG_ORIGINATE (INTRA_PREFIX))
//    zlog_debug ("Originate Intra-Area-Prefix-LSA for interface %s's prefix",
//                oi->interface->name);

  /* prepare buffer */
  memset (buffer, 0, sizeof (buffer));
  lsa_header = (struct ospf6_lsa_header *) buffer;
  intra_prefix_lsa = (struct ospf6_intra_prefix_lsa *)
                ((void *) lsa_header + sizeof (struct ospf6_lsa_header));

  /* Fill Intra-Area-Prefix-LSA */
  intra_prefix_lsa->ref_type = htons (OSPF6_LSTYPE_NETWORK);
  intra_prefix_lsa->ref_id = htonl (oi->interface->ifindex);
  intra_prefix_lsa->ref_adv_router = oi->area->ospf6->router_id;

  if (oi->state != OSPF6_INTERFACE_DR)
  {    
//    if (IS_OSPF6_DEBUG_ORIGINATE (INTRA_PREFIX))
//      zlog_debug ("  Interface is not DR");
    if (old)
      ospf6_lsa_purge (old);
    return 0;
  }    

  full_count = 0; 
  LIST_FOR_EACH(on, struct ospf6_neighbor, node, &oi->neighbor_list)
    if (on->state == OSPF6_NEIGHBOR_FULL)
      full_count++;

  if (full_count == 0)
  {
//    if (IS_OSPF6_DEBUG_ORIGINATE (INTRA_PREFIX))
//      zlog_debug ("  Interface is stub");
    if (old)
      ospf6_lsa_purge (old);
    return 0;
  }

  /* connected prefix to advertise */
  route_advertise = ospf6_route_table_create (0, 0);

  type = ntohs (OSPF6_LSTYPE_LINK);
  for (lsa = ospf6_lsdb_type_head (type, oi->lsdb); lsa;
       lsa = ospf6_lsdb_type_next (type, lsa))
  {
    if (OSPF6_LSA_IS_MAXAGE (lsa))
      continue;

//    if (IS_OSPF6_DEBUG_ORIGINATE (INTRA_PREFIX))
//      zlog_debug ("  include prefix from %s", lsa->name);

    if (lsa->header->adv_router != oi->area->ospf6->router_id)
    {
      on = ospf6_neighbor_lookup (lsa->header->adv_router, oi);
      if (on == NULL || on->state != OSPF6_NEIGHBOR_FULL)
      {
//        if (IS_OSPF6_DEBUG_ORIGINATE (INTRA_PREFIX))
//          zlog_debug ("    Neighbor not found or not Full, ignore");
        continue;
      }
    }

    link_lsa = (struct ospf6_link_lsa *)
        ((caddr_t) lsa->header + sizeof (struct ospf6_lsa_header));

    prefix_num = (unsigned short) ntohl (link_lsa->prefix_num);
    start = (char *) link_lsa + sizeof (struct ospf6_link_lsa);
    end = (char *) lsa->header + ntohs (lsa->header->length);
    for (current = start; current < end && prefix_num;
         current += OSPF6_PREFIX_SIZE (op))
    {
      op = (struct ospf6_prefix *) current;
      if (op->prefix_length == 0 ||
        current + OSPF6_PREFIX_SIZE (op) > end)
      break;

      route = ospf6_route_create ();
      route->type = OSPF6_DEST_TYPE_NETWORK;
      route->prefix.family = AF_INET6;
      route->prefix.prefixlen = op->prefix_length;
      memset (&route->prefix.u.prefix6, 0, sizeof (struct in6_addr));
      memcpy (&route->prefix.u.prefix6, OSPF6_PREFIX_BODY (op),
              OSPF6_PREFIX_SPACE (op->prefix_length));

      route->path.origin.type = lsa->header->type;
      route->path.origin.id = lsa->header->id;
      route->path.origin.adv_router = lsa->header->adv_router;
      route->path.options[0] = link_lsa->options[0];
      route->path.options[1] = link_lsa->options[1];
      route->path.options[2] = link_lsa->options[2];
      route->path.prefix_options = op->prefix_options;
      route->path.area_id = oi->area->area_id;
      route->path.type = OSPF6_PATH_TYPE_INTRA;

//      if (IS_OSPF6_DEBUG_ORIGINATE (INTRA_PREFIX))
//      {
//        prefix2str (&route->prefix, buf, sizeof (buf));
//        zlog_debug ("    include %s", buf);
//      }

      ospf6_route_add (route, route_advertise);
      prefix_num--;
    }
//    if(current != end && IS_OSPF6_DEBUG_ORIGINATE (INTRA_PREFIX))
//      zlog_debug ("Trailing garbage in %s", lsa->name);
  }

  op = (struct ospf6_prefix *)
            ((caddr_t) intra_prefix_lsa + sizeof (struct ospf6_intra_prefix_lsa));

  prefix_num = 0;
  for (route = ospf6_route_head (route_advertise); route;
       route = ospf6_route_best_next (route))
  {
    op->prefix_length = route->prefix.prefixlen;
    op->prefix_options = route->path.prefix_options;
    op->prefix_metric = htons (0);
    memcpy(OSPF6_PREFIX_BODY (op), &route->prefix.u.prefix6,
           OSPF6_PREFIX_SPACE (op->prefix_length));
    op = OSPF6_PREFIX_NEXT (op);
    prefix_num++;
  }

  ospf6_route_table_delete (route_advertise);

  if (prefix_num == 0)
  {
//    if (IS_OSPF6_DEBUG_ORIGINATE (INTRA_PREFIX))
//      zlog_debug ("Quit to Advertise Intra-Prefix: no route to advertise");
    return 0;
  }

  intra_prefix_lsa->prefix_num = htons (prefix_num);

  /* Fill LSA Header */
  lsa_header->age = 0;
  lsa_header->type = htons (OSPF6_LSTYPE_INTRA_PREFIX);
  lsa_header->id = htonl (oi->interface->ifindex);
  lsa_header->adv_router = oi->area->ospf6->router_id;
  lsa_header->seqnum =
  ospf6_new_ls_seqnum (lsa_header->type, lsa_header->id,
                                                             lsa_header->adv_router, oi->area->lsdb);
  lsa_header->length = htons ((caddr_t) op - (caddr_t) lsa_header);

  /* LSA checksum */
  ospf6_lsa_checksum (lsa_header);

  /* create LSA */
  lsa = ospf6_lsa_create (lsa_header);

  /* Originate */
  ospf6_lsa_originate_area (lsa, oi->area);


  return 0;
}

void ospf6_intra_prefix_lsa_add(struct ospf6_lsa * lsa)
{
  struct ospf6_area * oa;
  struct ospf6_intra_prefix_lsa * intra_prefix_lsa;
  struct prefix ls_prefix;
  struct ospf6_route *route, *ls_entry;
  int i, prefix_num;
  struct ospf6_prefix *op;
  char *start, *current, *end;

  if (OSPF6_LSA_IS_MAXAGE (lsa))
    return;
  
  oa = OSPF6_AREA (lsa->lsdb->data);

  intra_prefix_lsa = (struct ospf6_intra_prefix_lsa *)
          OSPF6_LSA_HEADER_END (lsa->header);
  if(intra_prefix_lsa->ref_type == htons (OSPF6_LSTYPE_ROUTER))
     ospf6_linkstate_prefix (intra_prefix_lsa->ref_adv_router,
                                            htonl (0), &ls_prefix);
  else if(intra_prefix_lsa->ref_type == htons (OSPF6_LSTYPE_NETWORK))
          ospf6_linkstate_prefix(intra_prefix_lsa->ref_adv_router,
                                 intra_prefix_lsa->ref_id, &ls_prefix);
  else
  {
//    if (IS_OSPF6_DEBUG_EXAMIN (INTRA_PREFIX))
//                                  zlog_debug ("Unknown reference LS-type: %#hx",
//    ntohs (intra_prefix_lsa->ref_type));
    return;
  }

  ls_entry = ospf6_route_lookup (&ls_prefix, oa->spf_table);
  if (ls_entry == NULL)
  {
//    if (IS_OSPF6_DEBUG_EXAMIN (INTRA_PREFIX))
//    {
//      ospf6_linkstate_prefix2str (&ls_prefix, buf, sizeof (buf));
//                                                            zlog_debug ("LS entry does not exist: %s", buf);
//    }
    return;
  }

  prefix_num = ntohs (intra_prefix_lsa->prefix_num);
  start = (caddr_t) intra_prefix_lsa +
                    sizeof (struct ospf6_intra_prefix_lsa);
  end = OSPF6_LSA_END (lsa->header);
  for (current = start; current < end; current += OSPF6_PREFIX_SIZE (op))
  {
    op = (struct ospf6_prefix *) current;
    if (prefix_num == 0)
      break;
    if (end < current + OSPF6_PREFIX_SIZE (op))
      break;

    route = ospf6_route_create ();

    memset (&route->prefix, 0, sizeof (struct prefix));
    route->prefix.family = AF_INET6;
    route->prefix.prefixlen = op->prefix_length;
    ospf6_prefix_in6_addr (&route->prefix.u.prefix6, op);

    route->type = OSPF6_DEST_TYPE_NETWORK;
    route->path.origin.type = lsa->header->type;
    route->path.origin.id = lsa->header->id;
    route->path.origin.adv_router = lsa->header->adv_router;
    route->path.prefix_options = op->prefix_options;
    route->path.area_id = oa->area_id;
    route->path.type = OSPF6_PATH_TYPE_INTRA;
    route->path.metric_type = 1;
    route->path.cost = ls_entry->path.cost +
    ntohs (op->prefix_metric);
    for(i = 0; ospf6_nexthop_is_set (&ls_entry->nexthop[i]) &&
         i < OSPF6_MULTI_PATH_LIMIT; i++)
      ospf6_nexthop_copy (&route->nexthop[i], &ls_entry->nexthop[i]);

//    if (IS_OSPF6_DEBUG_EXAMIN (INTRA_PREFIX))
//    {
//      prefix2str (&route->prefix, buf, sizeof (buf));
//                  zlog_debug ("  add %s", buf);
//    }

    ospf6_route_add (route, oa->route_table);
    prefix_num--;
  }

//  if (current != end && IS_OSPF6_DEBUG_EXAMIN (INTRA_PREFIX))
//    zlog_debug ("Trailing garbage ignored");

}

void ospf6_intra_prefix_lsa_remove(struct ospf6_lsa * lsa)
{

}

void ospf6_intra_route_calculation(struct ospf6_area * oa)
{
  struct ospf6_route * route;
  u_int16_t type;
  struct ospf6_lsa * lsa;
  void (*hook_add) (struct ospf6_route *) = NULL;
  void (*hook_remove) (struct ospf6_route *) = NULL;

  hook_add = oa->route_table->hook_add;
  hook_remove = oa->route_table->hook_remove;
  oa->route_table->hook_add = NULL;
  oa->route_table->hook_remove = NULL;

  for(route = ospf6_route_head (oa->route_table); route; route = ospf6_route_next (route))
    route->flag = OSPF6_ROUTE_REMOVE;

  type = htons (OSPF6_LSTYPE_INTRA_PREFIX);
  for (lsa = ospf6_lsdb_type_head (type, oa->lsdb); lsa; lsa = ospf6_lsdb_type_next (type, lsa))
    ospf6_intra_prefix_lsa_add (lsa);

  oa->route_table->hook_add = hook_add;
  oa->route_table->hook_remove = hook_remove;

  for(route = ospf6_route_head (oa->route_table); route; route = ospf6_route_next (route))
  {
    if(CHECK_FLAG (route->flag, OSPF6_ROUTE_REMOVE) &&
       CHECK_FLAG (route->flag, OSPF6_ROUTE_ADD))
    {
      UNSET_FLAG (route->flag, OSPF6_ROUTE_REMOVE);
      UNSET_FLAG (route->flag, OSPF6_ROUTE_ADD);
    }

    if(CHECK_FLAG (route->flag, OSPF6_ROUTE_REMOVE))
      ospf6_route_remove (route, oa->route_table);
    else if(CHECK_FLAG (route->flag, OSPF6_ROUTE_ADD) ||
            CHECK_FLAG (route->flag, OSPF6_ROUTE_CHANGE))
    {
      if(hook_add)
        (*hook_add) (route);
    }

    route->flag = 0;
  }
}
