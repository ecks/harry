extern "C" {
#include "stdlib.h"
#include "stdint.h"
#include "stdbool.h"

#include "riack.h"
#include "dblist.h"
#include "thread.h"
#include "routeflow-common.h"
#include "fake_ospf6_replica.h"
#include "ospf6_db.h"
#include "ospf6_top.h"
}

#include "CppUTest/TestHarness.h"


TEST_GROUP(ospf6_db_test)
{
  void setup()
  {
    ospf6_top_init();   
  }
};

TEST(ospf6_db_test, ospf6_header_put_get)
{
  struct ospf6_header * get_oh = (struct ospf6_header *)calloc(1, sizeof(struct ospf6_header));
  struct ospf6_header * put_oh = (struct ospf6_header *)calloc(1, sizeof(struct ospf6_header));
  unsigned int xid = 0;
  unsigned int id;

  get_oh->version = 5;
  get_oh->type = 7;
  get_oh->length = 10;
  get_oh->router_id = 11;
  get_oh->area_id = 12;
  get_oh->checksum = 21;
  get_oh->instance_id = 22;
  get_oh->reserved = 23;

  fake_ospf6_replica_set_id(1);

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
