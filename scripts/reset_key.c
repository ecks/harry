#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "riack.h"

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

int ingress_reset(riack_client * riack_client)
{
  riack_string bucket_type_str, bucket_str, key_str;
  riack_get_object * res_object;
  riack_object object;
   
  int curr_xid;
  char * curr_xid_str;

  bucket_type_str.value = "strongly_consistent2";
  bucket_type_str.len = strlen(bucket_type_str.value);

  bucket_str.value = "ids";
  bucket_str.len = strlen(bucket_str.value);

  key_str.value = "ingress_id2";
  key_str.len = strlen(key_str.value);

  if(riack_get_ext(riack_client, &bucket_str, &key_str, (riack_get_properties *)0, &bucket_type_str, &res_object, 0) != RIACK_SUCCESS)
  {
    fprintf(stderr, "riack_get failed\n");
    return -1;
  }

  if(res_object->object.content_count == 1)
  {
    object.vclock.len = res_object->object.vclock.len;

    object.vclock.clock = calloc(object.vclock.len, sizeof(void));
    memcpy(object.vclock.clock, res_object->object.vclock.clock, object.vclock.len);

    object.bucket.value = bucket_str.value;
    object.bucket.len = strlen(object.bucket.value);

    object.key.value = key_str.value;
    object.key.len = strlen(object.key.value);

    curr_xid = 0;

    // change to default value
    curr_xid_str = calloc(30, sizeof(char));
    sprintf(curr_xid_str, "%d", curr_xid);

    object.content = (riack_content *)calloc(1, sizeof(riack_content));

    memset(object.content, 0, sizeof(riack_content));

    object.content[0].content_type.value = "text/plain";
    object.content[0].content_type.len = strlen(object.content[0].content_type.value);
    object.content[0].data = (uint8_t *)curr_xid_str;
    object.content[0].data_len = strlen(curr_xid_str);

    if(riack_put_ext(riack_client, &object, &bucket_type_str, (riack_object **)0, (riack_put_properties *)0, 0) != RIACK_SUCCESS)
    {
      fprintf(stderr, "riack_put_ext failed\n");
      free(object.vclock.clock);
      free(curr_xid_str);
      return -1;
    }

    free(object.vclock.clock);
    free(curr_xid_str);
  }
}

int egress_reset(riack_client * riack_client)
{
  riack_string bucket_type_str, bucket_str, key_str;
  riack_get_object * res_object;
  riack_object object;
   
  int curr_xid;
  char * curr_xid_str;


  bucket_type_str.value = "strongly_consistent2";
  bucket_type_str.len = strlen(bucket_type_str.value);

  bucket_str.value = "ids";
//  sprintf(bucket_str.value, "%d", sibling_id); // need calloc beforehand
  bucket_str.len = strlen(bucket_str.value);

  key_str.value = "egress_id";
  key_str.len = strlen(key_str.value);

  if(riack_get_ext(riack_client, &bucket_str, &key_str, (riack_get_properties *)0, &bucket_type_str, &res_object, 0) != RIACK_SUCCESS)
  {
    fprintf(stderr, "riack_get failed\n");
    return -1;
  }

  if(res_object->object.content_count == 1)
  {
    object.vclock.len = res_object->object.vclock.len;

    object.vclock.clock = calloc(object.vclock.len, sizeof(void));
    memcpy(object.vclock.clock, res_object->object.vclock.clock, object.vclock.len);

    object.bucket.value = bucket_str.value;
    object.bucket.len = strlen(object.bucket.value);

    object.key.value = key_str.value;
    object.key.len = strlen(object.key.value);

    curr_xid = 0;

    // change to default value
    curr_xid_str = calloc(30, sizeof(char));
    sprintf(curr_xid_str, "%d", curr_xid);

    object.content = (riack_content *)calloc(1, sizeof(riack_content));

    memset(object.content, 0, sizeof(riack_content));

    object.content[0].content_type.value = "text/plain";
    object.content[0].content_type.len = strlen(object.content[0].content_type.value);
    object.content[0].data = (uint8_t *)curr_xid_str;
    object.content[0].data_len = strlen(curr_xid_str);

    if(riack_put_ext(riack_client, &object, &bucket_type_str, (riack_object **)0, (riack_put_properties *)0, 0) != RIACK_SUCCESS)
    {
      fprintf(stderr, "riack_put_ext failed\n");

      free(object.vclock.clock);
      free(curr_xid_str);
     
      return -1;
    }

    free(object.vclock.clock);
    free(curr_xid_str);
  }
}

int main(int argc, char * argv[])
{
  riack_client * riack_client;
  
  int curr_xid;
  char * host = "10.100.2.1";
  int port = 8087;
  char * curr_xid_str;

  riack_client = db_init(host, port);

  ingress_reset(riack_client); // reset db for ingress id
  egress_reset(riack_client); // reset db for egress xid sibling 0
//  egress_reset(riack_client, 1); // reset db for egress xid sibling 1
//  egress_reset(riack_client, 2); // reset db for egress xid sibling 2
}
