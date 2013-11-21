#include "stdio.h"
#include "stdbool.h"

#include "riack.h"

#include "dblist.h"
#include "routeflow-common.h"
#include "ospf6_top.h"

/* global ospf6d variable */
struct ospf6 *ospf6;

static struct ospf6 * ospf6_create(void)
{
  struct ospf6 * o;
  char * host = "127.0.0.1";
  int port = 8087;

  o = calloc(1, sizeof(struct ospf6));

  riack_init();
  o->riack_client = riack_new_client(0);
  if(riack_connect(o->riack_client, host, port, 0) != RIACK_SUCCESS) 
  {   
    printf("Failed to connect to riak server\n");
    riack_free(o->riack_client);
  }

  return o;
}

void ospf6_top_init()
{
  if(ospf6 == NULL)
    ospf6 = ospf6_create();
}
