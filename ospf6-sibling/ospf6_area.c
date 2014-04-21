#include "config.h"

#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <netinet/in.h>

#include "util.h"
#include "dblist.h"
#include "prefix.h"
#include "debug.h"
#include "thread.h"
#include "ospf6_proto.h"
#include "ospf6_lsa.h"
#include "ospf6_lsdb.h"
#include "ospf6_area.h"
#include "ospf6_top.h"
#include "ospf6_route.h"
#include "ospf6_spf.h"

/* schedule routing table recalculation */
static void ospf6_area_lsdb_hook_add(struct ospf6_lsa * lsa)
{
  switch (ntohs (lsa->header->type))
  {   
    case OSPF6_LSTYPE_ROUTER:
    case OSPF6_LSTYPE_NETWORK:
    if (IS_OSPF6_SIBLING_DEBUG_AREA)
    {   
      zlog_debug ("Examin %s", lsa->name);
      zlog_debug ("Schedule SPF Calculation for %s",
                   ((struct ospf6_area *)lsa->lsdb->data)->name);
    }   
    ospf6_spf_schedule(OSPF6_AREA(lsa->lsdb->data));
    break;

    case OSPF6_LSTYPE_INTRA_PREFIX:
      ospf6_intra_prefix_lsa_add (lsa);
      break;

    case OSPF6_LSTYPE_INTER_PREFIX:
    case OSPF6_LSTYPE_INTER_ROUTER:
//      ospf6_abr_examin_summary (lsa, (struct ospf6_area *) lsa->lsdb->data);
      break;

    default:
      break;
  }   

}

static void ospf6_area_lsdb_hook_remove(struct ospf6_lsa * lsa)
{
  switch(ntohs(lsa->header->type))
  {
    case OSPF6_LSTYPE_ROUTER:
    case OSPF6_LSTYPE_NETWORK:
    if (IS_OSPF6_SIBLING_DEBUG_AREA)
    {
      zlog_debug ("LSA disappearing: %s", lsa->name);
      zlog_debug ("Schedule SPF Calculation for %s",
      OSPF6_AREA (lsa->lsdb->data)->name);
    }
    ospf6_spf_schedule (OSPF6_AREA (lsa->lsdb->data));
    break;

  case OSPF6_LSTYPE_INTRA_PREFIX:
//    ospf6_intra_prefix_lsa_remove (lsa);
    break;

  case OSPF6_LSTYPE_INTER_PREFIX:
  case OSPF6_LSTYPE_INTER_ROUTER:
//    ospf6_abr_examin_summary (lsa, (struct ospf6_area *) lsa->lsdb->data);
    break;

  default:
    break;
  }
}

static void ospf6_area_route_hook_add (struct ospf6_route *route)
{
//  struct ospf6_route * copy = ospf6_route_copy(route);
//  ospf6_route_add(copy, ospf6->route_table);
}

static void
ospf6_area_route_hook_remove (struct ospf6_route *route)
{
//  struct ospf6_route * copy;

//  copy = ospf6_route_lookup_i
}

/* Make new area structure */
struct ospf6_area * ospf6_area_create (u_int32_t area_id, struct ospf6 *o)
{
  struct ospf6_area * oa;

  oa = calloc(1, sizeof(struct ospf6_area));

  oa->area_id = area_id;

  list_init(&oa->if_list);

  oa->lsdb = ospf6_lsdb_create(oa);
  oa->lsdb->hook_add = ospf6_area_lsdb_hook_add;
  oa->lsdb->hook_remove = ospf6_area_lsdb_hook_remove;
  oa->lsdb_self = ospf6_lsdb_create(oa);

  oa->spf_table = OSPF6_ROUTE_TABLE_CREATE(AREA, SPF_RESULTS);
  oa->spf_table->scope = oa;

  oa->route_table = OSPF6_ROUTE_TABLE_CREATE(AREA, ROUTES);
  oa->route_table->scope = oa;
  oa->route_table->hook_add = ospf6_area_route_hook_add;
  oa->route_table->hook_remove = ospf6_area_route_hook_remove;


  /* set default options */
  OSPF6_OPT_SET (oa->options, OSPF6_OPT_V6);
  OSPF6_OPT_SET (oa->options, OSPF6_OPT_E);
  OSPF6_OPT_SET (oa->options, OSPF6_OPT_R);

  oa->ospf6 = o;
  list_push_back(&o->area_list, &oa->node);

  return oa;
}

struct ospf6_area * ospf6_area_lookup (u_int32_t area_id, struct ospf6 *ospf6)
{
  struct ospf6_area * oa;

  LIST_FOR_EACH(oa, struct ospf6_area, node, &ospf6->area_list)
  {
    if(oa->area_id == area_id)
      return oa;
  }

  return (struct ospf6_area *)NULL;
}

