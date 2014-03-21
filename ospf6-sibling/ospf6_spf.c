#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "util.h"
#include "dblist.h"
#include "thread.h"
#include "ospf6_area.h"
#include "ospf6_spf.h"

extern struct thread_master * master;

static int ospf6_spf_calculation_thread(struct thread * t)
{
  return 0;
}

void ospf6_spf_schedule(struct ospf6_area * oa)
{
  if(oa->thread_spf_calculation)
    return;

  oa->thread_spf_calculation =
    thread_add_event(master, ospf6_spf_calculation_thread, oa, 0);
}
