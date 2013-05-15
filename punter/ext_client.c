#include "stdio.h"
#include "stdbool.h"
#include "stdlib.h"
#include "stdint.h"
#include "assert.h"
#include "sys/socket.h"

#include "util.h"
#include "routeflow-common.h"
#include "dblist.h"
#include "rfpbuf.h"
#include "ext_client_provider.h"

#define OSPF6_INST 0
#define BGP_INST   1

static struct ext_client_class * ext_client_classes[] = {
  &ospf6_ext_client_class,
  &bgp_ext_client_class,
};

static struct ext_client * ext_client_instances[] = {
  NULL,
  NULL,
};

void ext_client_init(char * host, struct punter_ctrl * punter_ctrl)
{
  int i;

  for(i = 0; i < ARRAY_SIZE(ext_client_classes); i++)
  {
    assert (ext_client_classes[i]->init != NULL);

    struct ext_client * ext_client = ext_client_classes[i]->init(host, punter_ctrl);
    ext_client_instances[i] = ext_client;   
  }
}

void ext_client_send(struct rfpbuf * obuf, enum rfp_type type)
{

  switch(type)
  {
    case RFPT_FORWARD_OSPF6:
      assert(ext_client_classes[OSPF6_INST]->send != NULL);
      ext_client_instances[OSPF6_INST]->obuf = rfpbuf_clone(obuf);
      ext_client_classes[OSPF6_INST]->send(ext_client_instances[OSPF6_INST]);
      break;

    case RFPT_FORWARD_BGP:
      assert(ext_client_classes[BGP_INST]->send != NULL);
      ext_client_instances[BGP_INST]->obuf = rfpbuf_clone(obuf);
      ext_client_classes[BGP_INST]->send(ext_client_instances[BGP_INST]);
      break;

    default:
      break;
  }
}

void ext_client_recv(struct ext_client * ext_client)
{
  punter_ext_to_zl_forward_msg(ext_client->ibuf); // punt the message over to zebralite
}
