#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <jansson.h>

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
  json_t * root;
  char * json_packed;
  unsigned int id;
  char * bucket = calloc(10, sizeof(char));
  char * key = calloc(10, sizeof(char));

  root = json_pack(
    "{s: i, s: i, s: i, s: i, s: i, s: i, s: i, s: i}",
    "version",
      oh->version,
    "type",
      oh->type,
    "length",
      oh->length,
    "router_id",
      oh->router_id,
    "area_id",
      oh->area_id,
    "checksum",
      oh->checksum,
    "instance_id",
      oh->instance_id,
    "reserved",
      oh->reserved  
    );

  json_packed = json_dumps(root, JSON_ENCODE_ANY);

  id = ospf6_replica_get_id();
  sprintf(bucket, "%d", id);

  sprintf(key, "%d", xid);

  if(IS_OSPF6_SIBLING_DEBUG_MSG)
  {
    zlog_debug("about to commit to database=> bucket: %s, key: %s, data: %s", bucket, key, json_packed);
  }

  riack_put_simple(ospf6->riack_client, bucket, key, json_packed, strlen(json_packed), "application/json");
  json_decref(root);

  free(json_packed);
  free(bucket);
  free(key);

  return 0;
}


int ospf6_db_fill(struct RIACK_CLIENT * riack_client, struct ospf6_header * oh, unsigned int xid, unsigned int bucket)
{
  json_t * root;
  struct RIACK_GET_OBJECT obj;
  size_t content_size;
  int ret;

  RIACK_STRING key, bucket_str;

  key.value = calloc(10, sizeof(char));
  bucket_str.value = calloc(10, sizeof(char));

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
      output_data[content_size] = '\0';
      printf("%s\n", output_data);

      json_parse(output_data);

      root = json_loads(output_data, 0, NULL);
      ret = json_unpack(root, "{s: i, s: i, s: i, s: i, s: i, s: i, s: i, s: i}",
   "version",
      &oh->version,
    "type",
      &oh->type,
    "length",
      &oh->length,
    "router_id",
      &oh->router_id,
    "area_id",
      &oh->area_id,
    "checksum",
      &oh->checksum,
    "instance_id",
      &oh->instance_id,
    "reserved",
      &oh->reserved  
  //        "router_id",
//          &oh->router_id,
//        "area_id",
//          &oh->area_id,
//        "version",
//          &oh->version,
//        "type",
//          &oh->type,
//        "length",
//          &oh->length,
//        "checksum",
//          &oh->checksum,
//        "instance_id",
//          &oh->instance_id,
//        "reserved",
//          &oh->reserved  
     );
      json_decref(root);

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
