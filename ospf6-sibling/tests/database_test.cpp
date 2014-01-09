extern "C" {
#include "stdlib.h"
#include "stdint.h"
#include "stdio.h"
#include "stdbool.h"
#include "arpa/inet.h"

#include "riack.h"
#include "dblist.h"
#include "thread.h"
#include "routeflow-common.h"
#include "fake_ospf6_replica.h"
#include "ospf6_proto.h"
#include "ospf6_db.h"
#include "ospf6_top.h"
}

#include "CppUTest/TestHarness.h"

static unsigned int xid = 0;
static unsigned int id = 1;

TEST_GROUP(ospf6_db_test)
{
  void setup()
  {
    ospf6_top_init();   
  }

  void teardown()
  {
    ospf6_db_delete(xid, id);
  }
};

TEST(ospf6_db_test, ospf6_header_put_get)
{
  struct ospf6_header * get_oh = (struct ospf6_header *)calloc(1, sizeof(struct ospf6_header) + sizeof(struct ospf6_hello) + 2*sizeof(u_int32_t));
  struct ospf6_header * put_oh = (struct ospf6_header *)calloc(1, sizeof(struct ospf6_header));

  struct ospf6_hello * hello;
  u_char * p;
  u_int32_t * router_id;

  int i;

  unsigned int id;

  get_oh->version = 5;
  get_oh->router_id = 11;
  get_oh->area_id = 12;
  get_oh->checksum = 21;
  get_oh->instance_id = 22;
  get_oh->reserved = 23;

  hello = (struct ospf6_hello *)((void *)get_oh + sizeof(struct ospf6_header));
 
  hello->interface_id = htonl(1);
  hello->priority = 1;
  
  OSPF6_OPT_SET(hello->options, OSPF6_OPT_V6);
  OSPF6_OPT_SET(hello->options, OSPF6_OPT_E);
  OSPF6_OPT_SET(hello->options, OSPF6_OPT_R);

  hello->hello_interval = htons (30);
  hello->dead_interval = htons (60);

  hello->drouter = htonl(0);
  hello->bdrouter = htonl(0);

  p = (u_char *)((void *)hello + sizeof(struct ospf6_hello));

  for(i = 0; i < 2; i++)
  {
    router_id = (u_int32_t *)p;
    p += sizeof(u_int32_t);
  
    *router_id = 1;
  }

  // do this at the end
  get_oh->type = OSPF6_MESSAGE_TYPE_HELLO;
  get_oh->length = htons(p - (u_char *)(get_oh));

  fake_ospf6_replica_set_id(id);

  ospf6_db_put_hello(get_oh, xid);

  id = ospf6_replica_get_id();

  ospf6_db_get(put_oh, xid, id);

  CHECK_EQUAL(get_oh->version, put_oh->version);
  CHECK_EQUAL(get_oh->type, put_oh->type);
  CHECK_EQUAL(get_oh->length, put_oh->length);
  CHECK_EQUAL(get_oh->router_id, put_oh->router_id);
  CHECK_EQUAL(get_oh->area_id, put_oh->area_id);
  CHECK_EQUAL(get_oh->checksum, put_oh->checksum);
  CHECK_EQUAL(get_oh->instance_id, put_oh->instance_id);
  CHECK_EQUAL(get_oh->reserved, put_oh->reserved);
}
