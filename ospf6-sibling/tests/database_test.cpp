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
#include "ospf6_lsa.h"
#include "ospf6_top.h"
}

#include "CppUTest/TestHarness.h"

static unsigned int hello_xid = 0;
static unsigned int dbdesc_xid = 1;

static unsigned int id = 1;

TEST_GROUP(ospf6_db_test)
{
  void setup()
  {
    ospf6_top_init(false);   
  }

  void teardown()
  {
    ospf6_db_delete(hello_xid, id);
    ospf6_db_delete(dbdesc_xid, id);
  }
};

TEST(ospf6_db_test, ospf6_hello_put_get)
{
  struct ospf6_header * get_oh = (struct ospf6_header *)calloc(1, sizeof(struct ospf6_header) + sizeof(struct ospf6_hello) + 2*sizeof(u_int32_t));
  struct ospf6_header * put_oh = (struct ospf6_header *)calloc(1, sizeof(struct ospf6_header));

  struct ospf6_hello * hello;
  u_char * p;
  u_int32_t * router_id;
  u_int32_t * get_router_id;
  u_int32_t * put_router_id;

  char * buffer;
  int i;

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

  ospf6_db_put_hello(get_oh, hello_xid);

  id = ospf6_replica_get_id();

  buffer = ospf6_db_get(put_oh, hello_xid, id); // set the header

  CHECK_EQUAL(get_oh->version, put_oh->version);
  CHECK_EQUAL(get_oh->type, put_oh->type);
  CHECK_EQUAL(get_oh->length, put_oh->length);
  CHECK_EQUAL(get_oh->router_id, put_oh->router_id);
  CHECK_EQUAL(get_oh->area_id, put_oh->area_id);
  CHECK_EQUAL(get_oh->checksum, put_oh->checksum);
  CHECK_EQUAL(get_oh->instance_id, put_oh->instance_id);
  CHECK_EQUAL(get_oh->reserved, put_oh->reserved);

  ospf6_db_fill_body(buffer, put_oh); // set the body

  CHECK_EQUAL(HELLO(get_oh)->interface_id, HELLO(put_oh)->interface_id);
  CHECK_EQUAL(HELLO(get_oh)->priority, HELLO(put_oh)->priority);
  CHECK_EQUAL(HELLO(get_oh)->options[0], HELLO(put_oh)->options[0]);
  CHECK_EQUAL(HELLO(get_oh)->options[1], HELLO(put_oh)->options[1]);
  CHECK_EQUAL(HELLO(get_oh)->options[2], HELLO(put_oh)->options[2]);
  CHECK_EQUAL(HELLO(get_oh)->hello_interval, HELLO(put_oh)->hello_interval);
  CHECK_EQUAL(HELLO(get_oh)->dead_interval, HELLO(put_oh)->dead_interval);
  CHECK_EQUAL(HELLO(get_oh)->drouter, HELLO(put_oh)->drouter);
  CHECK_EQUAL(HELLO(get_oh)->bdrouter, HELLO(put_oh)->bdrouter);

  for(i = 0; i < 2; i++)
  {
    get_router_id = HELLO_ROUTER_ID(get_oh, i);
    put_router_id = HELLO_ROUTER_ID(put_oh, i);

    CHECK_EQUAL(*get_router_id, *put_router_id);
    CHECK_EQUAL(*get_router_id, *put_router_id); 
  }
}

TEST(ospf6_db_test, ospf6_dbdesc_put_get)
{
  struct ospf6_header * get_oh = (struct ospf6_header *)calloc(1, sizeof(struct ospf6_header) + sizeof(struct ospf6_dbdesc) + 2*sizeof(struct ospf6_lsa_header));
  struct ospf6_header * put_oh = (struct ospf6_header *)calloc(1, sizeof(struct ospf6_header) + sizeof(struct ospf6_dbdesc) + 2*sizeof(struct ospf6_lsa_header));

  struct ospf6_dbdesc * dbdesc;
  struct ospf6_lsa_header * get_lsa_header;
  struct ospf6_lsa_header * put_lsa_header;
  u_char * p;

  char * buffer;
  int i;

  get_oh->version = 5;
  get_oh->router_id = 11;
  get_oh->area_id = 12;
  get_oh->checksum = 21;
  get_oh->instance_id = 22;
  get_oh->reserved = 23;

  dbdesc = (struct ospf6_dbdesc *)((void *)get_oh + sizeof(struct ospf6_header));

  OSPF6_OPT_SET (dbdesc->options, OSPF6_OPT_V6);
  OSPF6_OPT_SET (dbdesc->options, OSPF6_OPT_E);
  OSPF6_OPT_SET (dbdesc->options, OSPF6_OPT_R);

  dbdesc->bits |= OSPF6_DBDESC_MSBIT;
  dbdesc->bits |= OSPF6_DBDESC_MBIT;
  dbdesc->bits |= OSPF6_DBDESC_IBIT;

  dbdesc->ifmtu = htonl(1500);

  p = (u_char *)((void *)dbdesc + sizeof(struct ospf6_dbdesc));

  for(i = 0; i < 2; i++)
  {
    get_lsa_header = (struct ospf6_lsa_header *)p;
    get_lsa_header->age = 5;
    get_lsa_header->type = 6;
    get_lsa_header->id = 7;
    get_lsa_header->adv_router = 8;
    get_lsa_header->seqnum = 9;
    get_lsa_header->checksum = 10;
    get_lsa_header->length = 11;

    p += sizeof(struct ospf6_lsa_header);
  }

  // do this at the end
  get_oh->type = OSPF6_MESSAGE_TYPE_DBDESC;
  get_oh->length = htons(p - (u_char *)(get_oh));

  fake_ospf6_replica_set_id(id);

  ospf6_db_put_dbdesc(get_oh, dbdesc_xid);

  id = ospf6_replica_get_id();

  buffer = ospf6_db_get(put_oh, dbdesc_xid, id); // set the header

  CHECK_EQUAL(get_oh->version, put_oh->version);
  CHECK_EQUAL(get_oh->type, put_oh->type);
  CHECK_EQUAL(get_oh->length, put_oh->length);
  CHECK_EQUAL(get_oh->router_id, put_oh->router_id);
  CHECK_EQUAL(get_oh->area_id, put_oh->area_id);
  CHECK_EQUAL(get_oh->checksum, put_oh->checksum);
  CHECK_EQUAL(get_oh->instance_id, put_oh->instance_id);
  CHECK_EQUAL(get_oh->reserved, put_oh->reserved);

  ospf6_db_fill_body(buffer, put_oh); // set the body

  CHECK_EQUAL(DBDESC(get_oh)->reserved1, DBDESC(put_oh)->reserved1);
  CHECK_EQUAL(DBDESC(get_oh)->options[0], DBDESC(put_oh)->options[0]);
  CHECK_EQUAL(DBDESC(get_oh)->options[1], DBDESC(put_oh)->options[1]);
  CHECK_EQUAL(DBDESC(get_oh)->options[2], DBDESC(put_oh)->options[2]);
  CHECK_EQUAL(DBDESC(get_oh)->ifmtu, DBDESC(put_oh)->ifmtu);
  CHECK_EQUAL(DBDESC(get_oh)->reserved2, DBDESC(put_oh)->reserved2);
  CHECK_EQUAL(DBDESC(get_oh)->bits, DBDESC(put_oh)->bits);
  CHECK_EQUAL(DBDESC(get_oh)->seqnum, DBDESC(put_oh)->seqnum);

  // p is referenced to something else now
  p = (u_char *)((void *)put_oh + sizeof(struct ospf6_header) + sizeof(struct ospf6_dbdesc));
  
  put_lsa_header = (struct ospf6_lsa_header *)p;

  for(i = 0; i < 2; i++)
  {
    get_lsa_header = DBDESC_LSA_HEADER(get_oh, i);
    put_lsa_header = DBDESC_LSA_HEADER(put_oh, i);

    CHECK_EQUAL(get_lsa_header->age, put_lsa_header->age);
    CHECK_EQUAL(get_lsa_header->type, put_lsa_header->type);
    CHECK_EQUAL(get_lsa_header->id, put_lsa_header->id);
    CHECK_EQUAL(get_lsa_header->adv_router, put_lsa_header->adv_router);
    CHECK_EQUAL(get_lsa_header->seqnum, put_lsa_header->seqnum);
    CHECK_EQUAL(get_lsa_header->checksum, put_lsa_header->checksum);
    CHECK_EQUAL(get_lsa_header->length, put_lsa_header->length);
  }
}
