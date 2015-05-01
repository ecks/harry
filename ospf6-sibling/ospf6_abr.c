#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "util.h"
#include "dblist.h"
#include "riack.h"
#include "ospf6_top.h"
#include "ospf6_abr.h"
#include "ospf6_area.h"

int ospf6_is_router_abr(struct ospf6 * o)
{
  struct ospf6_area * oa;
  int area_count = 0;

  LIST_FOR_EACH(oa, struct ospf6_area, node, &o->area_list)
    if(IS_AREA_ENABLED(oa))
      area_count++;

  if(area_count > 1)
    return 1;
  return 0;
}

void ospf6_abr_enable_area(struct ospf6_area * area)
{

}
