#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "util.h"
#include "dblist.h"
#include "ospf6_proto.h"
#include "ospf6_top.h"
#include "ospf6_area.h"

/* Make new area structure */
struct ospf6_area * ospf6_area_create (u_int32_t area_id, struct ospf6 *o)
{
  struct ospf6_area * oa;

  oa = calloc(1, sizeof(struct ospf6_area));

  oa->area_id = area_id;

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

