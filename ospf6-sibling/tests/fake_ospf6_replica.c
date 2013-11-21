#include "config.h"

#include <stdbool.h>
#include <stdlib.h>
#include <netinet/in.h>

#include "dblist.h"
#include "prefix.h"
#include "fake_ospf6_replica.h"

static unsigned int g_id;

void fake_ospf6_replica_set_id(unsigned int id)
{
  g_id = id;
}

unsigned int ospf6_replica_get_id()
{
  return g_id;
}

int ospf6_leader_elect()
{
  return 0;
}
