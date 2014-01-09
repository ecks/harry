#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <riack.h>

#include "dblist.h"
#include "routeflow-common.h"
#include "debug.h"
#include "ospf6_top.h"
#include "ospf6_db.h"

/* global ospf6d variable */
struct ospf6 *ospf6;

int ospf6_db_put_hello(struct ospf6_header * oh, unsigned int xid)
{
  unsigned int id;
  char * bucket = calloc(ID_SIZE, sizeof(char));
  char * key = calloc(ID_SIZE, sizeof(char));

  char * json_header = calloc(DATA_SIZE, sizeof(char));
  char * json_msg = calloc(DATA_SIZE, sizeof(char));
  char * json_router_ids = calloc(DATA_SIZE, sizeof(char));
  char * json_packed = calloc(DATA_SIZE*3, sizeof(char));

  struct ospf6_hello * hello;
  u_char * p;

  hello = (struct ospf6_hello *)((void *)oh + sizeof(struct ospf6_header));

  sprintf(json_header, "{version: %d, type: %d, length: %d, router_id: %d, area_id: %d, checksum: %d, instance_id: %d, reserved: %d}", 
    oh->version,
    oh->type,
    oh->length,
    oh->router_id,
    oh->area_id,
    oh->checksum,
    oh->instance_id,
    oh->reserved);

  char * json_router_id_iter = json_router_ids;
  *json_router_id_iter = '[';
  json_router_id_iter++;
  bool first = true;

  for(p = (char *)((void *)hello + sizeof(struct ospf6_hello));
      p + sizeof(u_int32_t) <= OSPF6_MESSAGE_END(oh);
      p += sizeof(u_int32_t))
  {
    if(first)
      first = false;
    else if(!first)
    {
      *json_router_id_iter = ',';
      json_router_id_iter++;
      *json_router_id_iter = ' ';
      json_router_id_iter++;
    } 

    u_int32_t * router_id = (u_int32_t *)p;
    char * json_router_id = calloc(DATA_SIZE, sizeof(char));

    sprintf(json_router_id, "router_id: %d", *router_id);
    sprintf(json_router_id_iter, "%s", json_router_id);
    json_router_id_iter += strlen(json_router_id);

  }

  *json_router_id_iter = ']';

  sprintf(json_msg, "{interface_id: %d, priority: %d, options_0: %d, options_1: %d, options_2: %d, hello_interval: %d, dead_interval: %d, drouter: %d, bdrouter: %d, router_ids: %s}",
    hello->interface_id,
    hello->priority,
    hello->options[0],
    hello->options[1],
    hello->options[2],
    hello->hello_interval,
    hello->dead_interval,
    hello->drouter,
    hello->bdrouter,
    json_router_ids);

  sprintf(json_packed, "{header: %s, msg: %s}", json_header, json_msg); 

  id = ospf6_replica_get_id();
  sprintf(bucket, "%d", id);

  sprintf(key, "%d", xid);

  if(IS_OSPF6_SIBLING_DEBUG_MSG)
  {
    zlog_debug("about to commit to database=> bucket: %s, key: %s, data: %s", bucket, key, json_packed);
  }

  riack_put_simple(ospf6->riack_client, bucket, key, json_packed, strlen(json_packed), "application/json");

  free(json_packed);
  free(json_router_ids);
  free(json_msg);
  free(json_header);

  free(bucket);
  free(key);

  return 0;
}


int ospf6_db_fill(struct RIACK_CLIENT * riack_client, struct ospf6_header * oh, unsigned int xid, unsigned int bucket)
{
  struct RIACK_GET_OBJECT obj;
  size_t content_size;
  int ret;

  RIACK_STRING key, bucket_str;

  key.value = calloc(ID_SIZE, sizeof(char));
  bucket_str.value = calloc(ID_SIZE, sizeof(char));

  sprintf(bucket_str.value, "%d", bucket);
  bucket_str.len = strlen(bucket_str.value);

  sprintf(key.value, "%d", xid);
  key.len = strlen(key.value);


  if(riack_get(riack_client, bucket_str, key, 0, &obj) == RIACK_SUCCESS)
  {
    if(obj.object.content_count == 1)
    {
      content_size = obj.object.content[0].data_len;
      char * output_data = calloc(content_size, sizeof(char));
      strncpy(output_data, obj.object.content[0].data, content_size);
      printf("output data: %s\n", output_data);

      json_parse(output_data, oh);

      free(output_data);
    } 
  }

  free(key.value);
  free(bucket_str.value);
}

int ospf6_db_get(struct ospf6_header * oh, unsigned int xid, unsigned int bucket)
{
  int ret;

  ret = ospf6_db_fill(ospf6->riack_client, oh, xid, bucket);

  return ret;
}

int ospf6_db_delete(unsigned int xid, unsigned int bucket)
{
  RIACK_STRING key, bucket_str;

  key.value = calloc(ID_SIZE, sizeof(char));
  bucket_str.value = calloc(ID_SIZE, sizeof(char));

  sprintf(bucket_str.value, "%d", bucket);
  bucket_str.len = strlen(bucket_str.value);

  sprintf(key.value, "%d", xid);
  key.len = strlen(key.value);

  if(riack_delete(ospf6->riack_client, bucket_str, key, 0) == RIACK_SUCCESS)
  {
    return 0;
  }

  free(key.value);
  free(bucket_str.value);

  return -1;
}
