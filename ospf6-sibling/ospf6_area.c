#include "config.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <netinet/in.h>

#include "util.h"
#include "dblist.h"
#include "prefix.h"
#include "vector.h"
#include "vty.h"
#include "command.h"
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
  struct ospf6_route * copy = ospf6_route_copy(route);
  ospf6_route_add(copy, ospf6->route_table);
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

  oa->range_table = OSPF6_ROUTE_TABLE_CREATE(AREA, PREFIX_RANGES);
  oa->range_table->scope = oa;

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

static struct ospf6_area *
ospf6_area_get (u_int32_t area_id, struct ospf6 *o)
{
  struct ospf6_area *oa;
  oa = ospf6_area_lookup (area_id, o);
  if (oa == NULL)
    oa = ospf6_area_create (area_id, o);
  return oa;
}

#define OSPF6_CMD_AREA_GET(str, oa)                        \
{                                                          \
  u_int32_t area_id = 0;                                   \
  if (inet_pton (AF_INET, str, &area_id) != 1)             \
  {                                                        \
    zlog_debug("Malformed Area-ID: %s", str);    \
    return CMD_SUCCESS;                                    \
  }                                                        \
  oa = ospf6_area_get (area_id, ospf6);                    \
}


DEFUN (area_range,
       area_range_cmd,
       "area A.B.C.D range X:X::X:X/M",
       "OSPF area parameters\n"
       OSPF6_AREA_ID_STR
       "Configured address range\n"
       "Specify IPv6 prefix\n"
       )   
{
  int ret;
  struct ospf6_area *oa;
  struct prefix prefix;
  struct ospf6_route *range;

  OSPF6_CMD_AREA_GET (argv[0], oa);
  argc--;
  argv++;

  ret = str2prefix (argv[0], &prefix);
  if (ret != 1 || prefix.family != AF_INET6)
  {   
    zlog_debug("Malformed argument: %s", argv[0]);
    return CMD_SUCCESS;
  }   
  argc--;
  argv++;

  range = ospf6_route_lookup (&prefix, oa->range_table);
  if (range == NULL)
  {   
    range = ospf6_route_create (); 
    range->type = OSPF6_DEST_TYPE_RANGE;
    range->prefix = prefix;
  }   

  if (argc)
  {   
//    if (! strcmp (argv[0], "not-advertise"))
//      SET_FLAG (range->flag, OSPF6_ROUTE_DO_NOT_ADVERTISE);
//    else if (! strcmp (argv[0], "advertise"))
//      UNSET_FLAG (range->flag, OSPF6_ROUTE_DO_NOT_ADVERTISE);
  }   

  if (range->rnode)
  {   
    zlog_debug("Range already defined: %s%s", argv[-1]);
    return CMD_WARNING;
  }   

  ospf6_route_add (range, oa->range_table);
  return CMD_SUCCESS;
}

void ospf6_area_init(void)
{
  install_element (OSPF6_NODE, &area_range_cmd);
}
