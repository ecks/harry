extern "C" {
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>

#include <riack.h>

#include "dblist.h"
#include "prefix.h"
#include "routeflow-common.h"
#include "rfpbuf.h"
#include "rfp-msgs.h"
#include "rconn.h"
#include "vconn.h"
#include "ospf6_proto.h"
#include "ospf6_db.h"
}

#include "CppUTest/TestHarness.h"

#define TARGET "tcp6-fe80::20c:29ff:fe9e:ec01"

static rconn * rconn;
static RIACK_CLIENT * riack_client;

TEST_GROUP(controller_test)
{
  void setup()
  {
    char * host = "127.0.0.1";
    int port = 8087;

    rconn = rconn_create();
    rconn_connect(rconn, TARGET);
    rconn_run(rconn);

    riack_init();
    riack_client = riack_new_client(0);
    if(riack_connect(riack_client, host, port, 0) != RIACK_SUCCESS) 
    {   
      printf("Failed to connect to riak server\n");
      riack_free(riack_client);
    }
  }

};

TEST(controller_test, hello_put_get)
{
  int retval;
  struct rfpbuf * buffer;
  struct rfp_header * rh;
  struct rfp_router_features * rfr;
  struct rfp_phy_port * rpp;
  
  struct rfp_stats_routes * rsr;
  struct rfp_route * rr;
  struct rfp_connected_v4 * connected;

  int i;

  struct prefix_ipv4 p;

  struct ospf6_header * put_oh;
  struct ospf6_header * get_oh;
  struct ospf6_hello * hello;
  
  unsigned int xid = 0;

  u_char * ptr;

  if(inet_pton(AF_INET, "127.0.0.1", &(p.prefix.s_addr)) != -1)
  {
    p.prefixlen = 24;
  }

  for(i = 0; i < 4; i++)
  {
    buffer = rconn_recv(rconn);
  
    // get FEATURES_REQUEST
    if(buffer->size >= sizeof *rh)
    {
      rh = (struct rfp_header *)buffer->data;

      switch(rh->type)
      {
        case RFPT_FEATURES_REQUEST:
          printf("Receiving features request message\n");
          buffer = routeflow_alloc_xid(RFPT_FEATURES_REPLY, RFP10_VERSION, xid, sizeof(struct rfp_router_features));
          rfr = static_cast<struct rfp_router_features *>(buffer->l2);
          rfr->datapath_id = htonl(0);
          
          rpp = static_cast<struct rfp_phy_port *>(rfpbuf_put_uninit(buffer, sizeof(struct rfp_phy_port)));
          memset(rpp, 0, sizeof *rpp);
          rpp->port_no = htons(0);
          strncpy((char *)rpp->name, "eth3", sizeof rpp->name);
          rpp->state = htonl(RFPPS_FORWARD);
          rpp->mtu = htonl(1500);

          rfpmsg_update_length(buffer);
          retval = rconn_send(rconn, buffer);
          if(retval)
          {
            printf("retval failed\n");
          }
          break;

        case RFPT_ADDRESSES_REQUEST:
          printf("Receiving addresses request message\n");
          buffer = routeflow_alloc_xid(RFPT_ADDRESSES_REPLY, RFP10_VERSION, xid, sizeof(struct rfp_stats_routes));
          connected = static_cast<struct rfp_connected_v4 *>(rfpbuf_put_uninit(buffer, sizeof(struct rfp_connected_v4)));
          connected->type = AF_INET;
          connected->prefixlen = p.prefixlen;
          
          memcpy(&connected->p, &p.prefix, 4);

          connected->ifindex = 0;

          rfpmsg_update_length(buffer);
          retval = rconn_send(rconn, buffer);
          if(retval)
          {
            printf("retval failed\n");
          }
          break;

        case RFPT_STATS_ROUTES_REQUEST:
          printf("Receiving stats routes request message\n");
          buffer = routeflow_alloc_xid(RFPT_STATS_ROUTES_REPLY, RFP10_VERSION, xid, sizeof(struct rfp_stats_routes));
          rsr = static_cast<struct rfp_stats_routes *>(buffer->l2);

          rr = static_cast<struct rfp_route *>(rfpbuf_put_uninit(buffer, sizeof(struct rfp_route)));
          memset(rr, 0, sizeof *rr);
          rr->type = htons(1);
          rr->flags = htons(0);

          rr->prefixlen = htons(p.prefixlen);
          memcpy(&rr->p, &p.prefix, 4);
          rr->metric = htons(0);
          rr->distance = htonl(0);
          rr->table = htonl(0);

          rfpmsg_update_length(buffer);
          retval = rconn_send(rconn, buffer);
          if(retval)
          {
            printf("retval failed\n");
          }
          break;

        case RFPT_FORWARD_OSPF6:
          printf("Forwarding OSPF6 traffic\n");
          break;
      }
    }
  }

  buffer = routeflow_alloc_xid(RFPT_FORWARD_OSPF6, RFP10_VERSION, xid, sizeof(struct rfp_header));
  rh = static_cast<struct rfp_header *>(buffer->l2);

  put_oh = static_cast<struct ospf6_header *>(rfpbuf_put_uninit(buffer, sizeof(struct ospf6_header)));

  hello = static_cast<struct ospf6_hello *>(rfpbuf_put_uninit(buffer, sizeof(struct ospf6_hello)));

  hello->interface_id = htonl(0);
  hello->priority = 0;

  OSPF6_OPT_SET(hello->options, OSPF6_OPT_V6);
  OSPF6_OPT_SET(hello->options, OSPF6_OPT_E);
  OSPF6_OPT_SET(hello->options, OSPF6_OPT_R);

  hello->hello_interval = htons(10);
  hello->dead_interval = htons(40);

  hello->drouter = htonl(0);
  hello->bdrouter = htonl(0);

  ptr = static_cast<u_char *>((static_cast<void *>(hello + sizeof(struct ospf6_hello))));

  // do this at the end 
  put_oh->type = OSPF6_MESSAGE_TYPE_HELLO;
  put_oh->length = htons(ptr - (u_char *)(put_oh));

  // fill ospf6 header
  put_oh->version = OSPFV3_VERSION;
  put_oh->router_id = 0;
  put_oh->area_id = 0;
  put_oh->instance_id = htons(0);
  put_oh->reserved = htons(0);

  rfpmsg_update_length(buffer);

  retval = rconn_send(rconn, buffer);
  if(retval)
  {
    printf("retval failed\n");
  }

  // retrieve value from db directly
  get_oh = static_cast<struct ospf6_header *>(calloc(1, sizeof(struct ospf6_header)));

  // id of 0
  ospf6_db_fill(riack_client, get_oh, xid, 0);


  CHECK_EQUAL(get_oh->version, put_oh->version);
  CHECK_EQUAL(get_oh->type, put_oh->type);
  CHECK_EQUAL(get_oh->length, put_oh->length);
  CHECK_EQUAL(get_oh->router_id, put_oh->router_id);
  CHECK_EQUAL(get_oh->area_id, put_oh->area_id);
  CHECK_EQUAL(get_oh->checksum, put_oh->checksum);
  CHECK_EQUAL(get_oh->instance_id, put_oh->instance_id);
  CHECK_EQUAL(get_oh->reserved, put_oh->reserved);
}
