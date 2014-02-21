#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <riack.h>

#include "dblist.h"
#include "db.h"
#include "routeflow-common.h"
#include "debug.h"
#include "ospf6_lsa.h"
#include "ospf6_top.h"
#include "ospf6_db.h"

/* global ospf6d variable */
struct ospf6 *ospf6;

char * ospf6_db_json_header(struct ospf6_header * oh)
{
  char * json_header = calloc(DATA_SIZE, sizeof(char));

  sprintf(json_header, "{version: %d, type: %d, length: %d, router_id: %d, area_id: %d, checksum: %d, instance_id: %d, reserved: %d}", 
    oh->version,
    oh->type,
    oh->length,
    oh->router_id,
    oh->area_id,
    oh->checksum,
    oh->instance_id,
    oh->reserved);

  return json_header;
}

int ospf6_db_put_hello(struct ospf6_header * oh, unsigned int xid)
{
  unsigned int id;
  char * bucket;
  char * key;

  char * json_header;
  char * json_msg;
  char * json_router_ids;
  char * json_packed;

  struct ospf6_hello * hello;
  u_char * p;

  struct RIACK_PAIR * indexes;
  struct RIACK_CONTENT content;
  struct RIACK_OBJECT object;

  /* initialize memory buffers */
  bucket = calloc(ID_SIZE, sizeof(char));
  key = calloc(ID_SIZE, sizeof(char));
  json_msg = calloc(DATA_SIZE*2, sizeof(char));
  json_router_ids = calloc(DATA_SIZE, sizeof(char));
  json_packed = calloc(DATA_SIZE*3, sizeof(char));

  json_header = ospf6_db_json_header(oh);

  hello = (struct ospf6_hello *)((void *)oh + sizeof(struct ospf6_header));

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

    sprintf(json_router_id, "{router_id: %d}", *router_id);
    sprintf(json_router_id_iter, "%s", json_router_id);
    json_router_id_iter += strlen(json_router_id);

    free(json_router_id);

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

  sprintf(json_packed, "{header: %s, hello: %s}", json_header, json_msg); 

  id = ospf6_replica_get_id();
  sprintf(bucket, "%d", id);

  sprintf(key, "%d", xid);

  if(IS_OSPF6_SIBLING_DEBUG_MSG)
  {
    zlog_debug("about to commit to database=> bucket: %s, key: %s, data: %s", bucket, key, json_packed);
  }

  indexes = calloc(1, sizeof(struct RIACK_PAIR));
  indexes[0].key.value = INDEX_KEY;
  indexes[0].key.len = strlen(indexes[0].key.value);
  indexes[0].value_present = 1;

  indexes[0].value_len = strlen(key);
  indexes[0].value = (uint8_t*)key;

  memset(&content, 0, sizeof(content));
  memset(&object, 0, sizeof(object));

  object.bucket.value = bucket;
  object.bucket.len = strlen(bucket);
  object.key.value = key;
  object.key.len = strlen(key);
  object.content_count = 1;
  object.content = &content;

  content.content_type.value = "application/json";
  content.content_type.len = strlen(content.content_type.value);
  content.index_count = 1;
  content.indexes = indexes;
  content.data = (uint8_t*)json_packed;
  content.data_len = strlen(json_packed);

  riack_put(ospf6->riack_client, object, 0, 0);

  free(indexes);
  free(json_packed);
  free(json_router_ids);
  free(json_msg);
  free(json_header);

  free(bucket);
  free(key);

  return 0;
}

int ospf6_db_put_dbdesc(struct ospf6_header * oh, unsigned int xid)
{
  unsigned int id;
  char * bucket;
  char * key;

  char * json_header;
  char * json_msg;
  char * json_lsa_headers;
  char * json_packed;

  struct ospf6_dbdesc * dbdesc;
  u_char * p;

  struct RIACK_PAIR * indexes;
  struct RIACK_CONTENT content;
  struct RIACK_OBJECT object;

  /* initialize memory buffers */
  bucket = calloc(ID_SIZE, sizeof(char));
  key = calloc(ID_SIZE, sizeof(char));
  json_msg = calloc(DATA_SIZE*2, sizeof(char));
  json_lsa_headers = calloc(DATA_SIZE, sizeof(char));
  json_packed = calloc(DATA_SIZE*3, sizeof(char));

  json_header = ospf6_db_json_header(oh);

  dbdesc = (struct ospf6_dbdesc *)((void *)oh + sizeof(struct ospf6_header));

  char * json_lsa_header_iter = json_lsa_headers;
  *json_lsa_header_iter = '[';
  json_lsa_header_iter++;
  bool first = true;

  /* LSA headers */
  for(p = (char *)((void *)dbdesc + sizeof(struct ospf6_dbdesc));
      p + sizeof(struct ospf6_lsa_header) <= ((void *)(oh) + ntohs((oh)->length));
      p += sizeof(struct ospf6_lsa_header))
  {
    if(first)
      first = false;
    else if(!first)
    {
      *json_lsa_header_iter = ',';
      json_lsa_header_iter++;
      *json_lsa_header_iter = ' ';
      json_lsa_header_iter++;
    }

    struct ospf6_lsa_header * lsa_header = (struct ospf6_lsa_header *)p;
    char * json_lsa_header = calloc(DATA_SIZE, sizeof(char));

    sprintf(json_lsa_header, "{age: %d, type: %d, id: %d, adv_router: %d, seqnum: %d, checksum: %d, length: %d}", 
      lsa_header->age, 
      lsa_header->type, 
      lsa_header->id, 
      lsa_header->adv_router,
      lsa_header->seqnum,
      lsa_header->checksum,
      lsa_header->length);
    sprintf(json_lsa_header_iter, "%s", json_lsa_header);
    json_lsa_header_iter += strlen(json_lsa_header);

    free(json_lsa_header);
  }

  *json_lsa_header_iter = ']';

  sprintf(json_msg, "{reserved1: %d, options_0: %d, options_1: %d, options_2: %d, ifmtu: %d, reserved2: %d, bits: %d, seqnum: %d, lsa_headers: %s}",
    dbdesc->reserved1,
    dbdesc->options[0],
    dbdesc->options[1],
    dbdesc->options[2],
    dbdesc->ifmtu,
    dbdesc->reserved2,
    dbdesc->bits,
    dbdesc->seqnum,
    json_lsa_headers);

  sprintf(json_packed, "{header: %s, dbdesc: %s}", json_header, json_msg);

  id = ospf6_replica_get_id();
  sprintf(bucket, "%d", id);

  sprintf(key, "%d", xid);

  if(IS_OSPF6_SIBLING_DEBUG_MSG)
  {
    zlog_debug("about to commit to database=> bucket: %s, key: %s, data: %s", bucket, key, json_packed);
  }

  indexes = calloc(1, sizeof(struct RIACK_PAIR));
  indexes[0].key.value = "index1_int";
  indexes[0].key.len = strlen(indexes[0].key.value);
  indexes[0].value_present = 1;

  indexes[0].value_len = strlen(key);
  indexes[0].value = (uint8_t*)key;

  memset(&content, 0, sizeof(content));
  memset(&object, 0, sizeof(object));

  object.bucket.value = bucket;
  object.bucket.len = strlen(bucket);
  object.key.value = key;
  object.key.len = strlen(key);
  object.content_count = 1;
  object.content = &content;

  content.content_type.value = "application/json";
  content.content_type.len = strlen(content.content_type.value);
  content.index_count = 1;
  content.indexes = indexes;
  content.data = (uint8_t*)json_packed;
  content.data_len = strlen(json_packed);

  riack_put(ospf6->riack_client, object, 0, 0);

  free(json_packed);
  free(json_lsa_headers);
  free(json_msg);
  free(json_header);

  free(bucket);
  free(key);

  return 0;
}

char * ospf6_db_fill_header(struct RIACK_CLIENT * riack_client, struct ospf6_header * oh, unsigned int xid, unsigned int bucket)
{
  char * output_data;
  struct RIACK_GET_OBJECT obj;
  size_t content_size;
  int ret;

  // in case there is nothing to get
  output_data = NULL;

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
      output_data = calloc(content_size, sizeof(char));
      strncpy(output_data, obj.object.content[0].data, content_size);
      if(OSPF6_SIBLING_DEBUG_RESTART)
      {
        zlog_debug("output data: %s", output_data);
      }
      json_parse_header(output_data, oh);
    } 
  }

  free(key.value);
  free(bucket_str.value);

  return output_data;
}

char * ospf6_db_get(struct ospf6_header * oh, unsigned int xid, unsigned int bucket)
{
  return ospf6_db_fill_header(ospf6->riack_client, oh, xid, bucket);
}

// void * here would be pointer to 
//   struct ospf6_hello *
//   struct ospf6_dbdesc *
int ospf6_db_fill_body(char * buffer, void * body)
{
  json_parse_body(buffer, body);

  return 0;
}

extern struct keys * ospf6_db_list_keys(unsigned int bucket)
{
  return db_list_keys(ospf6->riack_client, bucket, true);
}

extern struct keys * ospf6_db_range_keys(unsigned int bucket, unsigned int start_xid, unsigned int end_xid)
{
  RIACK_STRING bucket_str, index, min_xid, max_xid;
  RIACK_STRING_LIST list;
  struct keys * keys = NULL;
  int i;

  char * cur_str;
  unsigned int cur_str_len;

  index.value = INDEX_KEY;
  index.len = strlen(index.value);
  
  bucket_str.value = calloc(ID_SIZE, sizeof(char));
  sprintf(bucket_str.value, "%d", bucket);
  bucket_str.len = strlen(bucket_str.value);

  min_xid.value = calloc(ID_SIZE, sizeof(char));
  sprintf(min_xid.value, "%d", start_xid);
  min_xid.len = strlen(min_xid.value);

  max_xid.value = calloc(ID_SIZE, sizeof(char));
  sprintf(max_xid.value, "%d", end_xid);
  max_xid.len = strlen(max_xid.value);

  
  if(riack_2i_query_range(ospf6->riack_client, bucket_str, index, min_xid, max_xid, &list) == RIACK_SUCCESS)
  {
    if(OSPF6_SIBLING_DEBUG_RESTART)
    {
      zlog_debug("range query executed successfully with bucket: %s, min_xid: %s, max_xid: %s", bucket_str.value, min_xid.value, max_xid.value);
    }

    keys = calloc(1, sizeof(struct keys));
    keys->num_keys = list.string_count;
    keys->key_str_ptrs = calloc(list.string_count, sizeof(char *));

    for(i = 0; i < list.string_count; i++)
    {
      cur_str = list.strings[i].value;
      cur_str_len = list.strings[i].len;
      keys->key_str_ptrs[i] = calloc(cur_str_len + 1, sizeof(char)); // 1 extra for null byte
      strncpy(keys->key_str_ptrs[i], cur_str, cur_str_len);
      keys->key_str_ptrs[i][cur_str_len] = '\0';
    }
  }

  free(bucket_str.value);
  free(min_xid.value);
  free(max_xid.value);

  return keys;
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
