#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <riack.h>

#include "db.h"

extern riack_client * db_init(char * host, int port)
{
  riack_client * riack_client;

  riack_init();
  riack_client = riack_new_client(0);
  if(riack_connect(riack_client, host, port, 0) != RIACK_SUCCESS)
  {
    printf("Failed to connect to riak server\n");
    riack_free(riack_client);
  }

  return riack_client;
}

extern void db_destroy(riack_client * riack_client)
{
  if(riack_disconnect(riack_client) != RIACK_SUCCESS)
  {
    printf("Failed to disconnect from riak server\n");
    riack_free(riack_client);
  }

  riack_cleanup();
  riack_free(riack_client);
}

int keys_compare(const void * a, const void * b)
{
  char ** a_str_ptr = (char **)a;
  char ** b_str_ptr = (char **)b;

  char * a_str = *a_str_ptr;
  char * b_str = *b_str_ptr;
 
  unsigned int a_xid, b_xid;

  sscanf(a_str, "%d", &a_xid);
  sscanf(b_str, "%d", &b_xid);

  if(a_xid < b_xid)
    return -1;
  else if(a_xid > b_xid)
    return 1;
  else
    if(b_xid == b_xid)
      return 0;
}

extern struct keys * db_list_keys(riack_client * riack_client, unsigned int bucket, bool sort_req)
{
  riack_string_linked_list * list;
  riack_string_linked_list * current;
  riack_string bucket_str;
  size_t list_size;

  size_t cur_str_len;
  struct keys * keys = NULL;

  bucket_str.value = calloc(ID_SIZE, sizeof(char));

  sprintf(bucket_str.value, "%d", bucket);
  bucket_str.len = strlen(bucket_str.value);

  if(riack_list_keys(riack_client, &bucket_str, &list) == RIACK_SUCCESS)
  {
    list_size = riack_string_linked_list_size(&list);

    keys = calloc(1, sizeof(keys));
    keys->num_keys = list_size;
    keys->key_str_ptrs = calloc(list_size, sizeof(char *));

    current = list;
    int i = 0;

    while(current != NULL)
    {
      cur_str_len = current->string.len;
      keys->key_str_ptrs[i] = calloc(cur_str_len + 1, sizeof(char)); // 1 extra for NULL byte
      strncpy(keys->key_str_ptrs[i], current->string.value, cur_str_len);
      keys->key_str_ptrs[i][cur_str_len] = '\0';
      
      current = current->next;

      i++;
    }
 
    if(sort_req)
    {
      qsort(keys->key_str_ptrs, keys->num_keys, sizeof(char *), keys_compare);
    }

    riack_free_string_linked_list_p(riack_client, &list);
  }
  else
  {
    // keys still needs to be initialized
    keys = calloc(1, sizeof(keys));
    keys->num_keys = 0;
    keys->key_str_ptrs = NULL;
  }

  free(bucket_str.value);
  return keys;
}

extern int db_get_unique_id(riack_client * riack_client)
{
  riack_string bucket_type_str, bucket_str, key_str;

  bucket_type_str.value = "strongly_consistent";
  bucket_type_str.len = strlen(bucket_type_str.value);

  bucket_str.value = "ids";
  bucket_str.len = strlen(bucket_str.value);

  key_str.value = "ingress_id";
  key_str.len = strlen(key_str.value);

  return db_get_and_set(riack_client, &bucket_type_str, &bucket_str, &key_str);
}

int db_get_and_set(riack_client * riack_client, riack_string * bucket_type_str, riack_string * bucket_str, riack_string * key_str)
{
  riack_object object;
  riack_get_object * res_object;

  size_t data_len;
  int curr_xid;
  char * curr_xid_str, * output_data;

  if(riack_get_ext(riack_client, bucket_str, key_str, (riack_get_properties *)0, bucket_type_str, &res_object, 0) != RIACK_SUCCESS)
  {
    fprintf(stderr, "riack_get failed\n");
    return -1;
  }
 
  if(res_object->object.content_count == 1)
  {
    data_len = res_object->object.content[0].data_len;
    output_data = calloc(30, sizeof(char));
    strncpy(output_data, res_object->object.content[0].data, data_len);
    printf("%s\n", output_data);

    printf("%d\n", (int)    res_object->object.vclock.len);
    printf("%d\n", (int) *(res_object->object.vclock.clock));


    object.vclock.len = res_object->object.vclock.len;

    object.vclock.clock = calloc(object.vclock.len, sizeof(void));

    memcpy(object.vclock.clock, res_object->object.vclock.clock, object.vclock.len);

    object.bucket.value = bucket_str->value;
    object.bucket.len = strlen(object.bucket.value);

    object.key.value = key_str->value;
    object.key.len = strlen(object.key.value);

    curr_xid = (int)strtol(output_data, NULL, 10);
    curr_xid++;

    // change to default value
    curr_xid_str = calloc(30, sizeof(char));
    sprintf(curr_xid_str, "%d", curr_xid);

    object.content = (riack_content *)calloc(1, sizeof(riack_content));

    memset(object.content, 0, sizeof(riack_content));

    object.content[0].content_type.value = "text/plain";
    object.content[0].content_type.len = strlen(object.content[0].content_type.value);
    object.content[0].data = (uint8_t *)curr_xid_str;
    object.content[0].data_len = strlen(curr_xid_str);

    if(riack_put_ext(riack_client, &object, bucket_type_str, (riack_object **)0, (riack_put_properties *)0, 0) != RIACK_SUCCESS)
    {
      fprintf(stderr, "riack_put_ext failed\n");
      return -1;
    }

    return curr_xid;
  }

  return 0;
}

int db_get_replica_xid(riack_client * riack_client, unsigned int replica_id)
{
  riack_string bucket_type_str, bucket_str, key_str;

  bucket_type_str.value = "strongly_consistent2";
  bucket_type_str.len = strlen(bucket_type_str.value);

  bucket_str.value = calloc(30, sizeof(char));
  sprintf(bucket_str.value, "%d", replica_id);
  bucket_str.len = strlen(bucket_str.value);

  key_str.value = "egress_xid";
  key_str.len = strlen(key_str.value);

  riack_object object;
  riack_get_object * res_object;

  size_t data_len;
  int curr_xid;
  char * output_data;

  if(riack_get_ext(riack_client, &bucket_str, &key_str, (riack_get_properties *)0, &bucket_type_str, &res_object, 0) != RIACK_SUCCESS)
  {
    fprintf(stderr, "riack_get failed\n");
    return -1;
  }
 
  if(res_object->object.content_count == 1)
  {
    data_len = res_object->object.content[0].data_len;
    output_data = calloc(30, sizeof(char));
    strncpy(output_data, res_object->object.content[0].data, data_len);
    printf("%s\n", output_data);

    printf("%d\n", (int)    res_object->object.vclock.len);
    printf("%d\n", (int) *(res_object->object.vclock.clock));


    object.vclock.len = res_object->object.vclock.len;

    object.vclock.clock = calloc(object.vclock.len, sizeof(void));

    memcpy(object.vclock.clock, res_object->object.vclock.clock, object.vclock.len);

    object.bucket.value = bucket_str.value;
    object.bucket.len = strlen(object.bucket.value);

    object.key.value = key_str.value;
    object.key.len = strlen(object.key.value);

    curr_xid = (int)strtol(output_data, NULL, 10);

    // deallocate
    free(bucket_str.value);
    free(object.vclock.clock);

    return curr_xid;
  }
  
  free(bucket_str.value);
  return 0;
}


extern int db_set_replica_xid(riack_client * riack_client, unsigned int replica_id, unsigned int xid, bool checkpoint_egress_xid)
{
  zlog_debug("Entering db_set_replica_xid");

  riack_string bucket_type_str, bucket_str, key_str;

  bucket_type_str.value = "strongly_consistent2";
  bucket_type_str.len = strlen(bucket_type_str.value);

  bucket_str.value = calloc(30, sizeof(char));
  sprintf(bucket_str.value, "%d", replica_id); // need calloc beforehand
  bucket_str.len = strlen(bucket_str.value);

  key_str.value = "egress_xid";
  key_str.len = strlen(key_str.value);

  riack_object object;
  riack_get_object * res_object;

  size_t data_len;
  int curr_xid;
  char * curr_xid_str, * output_data;


  zlog_debug("db_set_replica_xid: before retrieving objects: replica id: %d, xid: %d, bucket_type_str: %s, bucket_str: %s, key_str: %s", replica_id, xid, bucket_type_str.value, bucket_str.value, key_str.value);

  if(riack_get_ext(riack_client, &bucket_str, &key_str, (riack_get_properties *)0, &bucket_type_str, &res_object, 0) != RIACK_SUCCESS)
  {
    zlog_debug("db_set_replica_xid: after retrieving objects ==> failure, error: %s", riack_client->last_error);
    fprintf(stderr, "riack_get failed\n");

    int i = 0;
 
    return -1;
  }
 
  zlog_debug("db_set_replica_xid: after retrieving objects ==> success, last error: %s", riack_client->last_error);

  if(res_object->object.content_count == 1)
  {
    data_len = res_object->object.content[0].data_len;
    output_data = calloc(30, sizeof(char));
    strncpy(output_data, res_object->object.content[0].data, data_len);
    printf("%s\n", output_data);

    printf("%d\n", (int)    res_object->object.vclock.len);
    printf("%d\n", (int) *(res_object->object.vclock.clock));


    object.vclock.len = res_object->object.vclock.len;

    object.vclock.clock = calloc(object.vclock.len, sizeof(void));

    memcpy(object.vclock.clock, res_object->object.vclock.clock, object.vclock.len);

    object.bucket.value = bucket_str.value;
    object.bucket.len = strlen(object.bucket.value);

    object.key.value = key_str.value;
    object.key.len = strlen(object.key.value);

    curr_xid = (int)strtol(output_data, NULL, 10);

    // change to default value
    curr_xid_str = calloc(30, sizeof(char));
    sprintf(curr_xid_str, "%d", xid);

    object.content = (riack_content *)calloc(1, sizeof(riack_content));

    memset(object.content, 0, sizeof(riack_content));

    object.content[0].content_type.value = "text/plain";
    object.content[0].content_type.len = strlen(object.content[0].content_type.value);
    object.content[0].data = (uint8_t *)curr_xid_str;
    object.content[0].data_len = strlen(curr_xid_str);

    zlog_debug("db_set_replica_xid: before putting objects");

    if(riack_put_ext(riack_client, &object, &bucket_type_str, (riack_object **)0, (riack_put_properties *)0, 0) != RIACK_SUCCESS)
    {
      zlog_debug("db_set_replica_xid: after putting objects ==> failure, error: %s", riack_client->last_error);

      fprintf(stderr, "riack_put_ext failed\n");
      free(bucket_str.value);
      free(object.vclock.clock);
      return -1;
    }
 
    zlog_debug("db_set_replica_xid: after putting objects ==> success");

    free(bucket_str.value);
    free(object.vclock.clock);

    return 0;
  }

  return -1;
}

extern struct keys * db_range_keys(riack_client * riack_client, unsigned int bucket, unsigned int start_xid, unsigned int end_xid, bool sort_req)
{
  riack_string bucket_str, index, min_xid, max_xid;
  riack_string_list * list;
  struct keys * keys = NULL;
  size_t list_size;
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

  
  if(riack_2i_query_range(riack_client, &bucket_str, &index, &min_xid, &max_xid, &list) == RIACK_SUCCESS)
  {
    list_size = list->string_count;

    keys = calloc(1, sizeof(struct keys));
    keys->num_keys = list_size;
    keys->key_str_ptrs = calloc(list_size, sizeof(char *));

    for(i = 0; i < list_size; i++)
    {
      cur_str = list->strings[i].value;
      cur_str_len = list->strings[i].len;
      keys->key_str_ptrs[i] = calloc(cur_str_len + 1, sizeof(char)); // 1 extra for null byte
      strncpy(keys->key_str_ptrs[i], cur_str, cur_str_len);
      keys->key_str_ptrs[i][cur_str_len] = '\0';
    }

    if(sort_req)
    {
      qsort(keys->key_str_ptrs, keys->num_keys, sizeof(char *), keys_compare);
    }
  }
  else
  {
    // when there are no keys, still need to initialize keys struct
    keys = calloc(1, sizeof(struct keys));
    keys->num_keys = 0;
    keys->key_str_ptrs = NULL;
  }

  free(bucket_str.value);
  free(min_xid.value);
  free(max_xid.value);

  return keys;

}

extern void db_free_keys(struct keys * keys)
{
  int i;
  for(i = 0; i < keys->num_keys; i++)
  {
    free(keys->key_str_ptrs[i]);
  }

  free(keys);
  keys = NULL;
}
