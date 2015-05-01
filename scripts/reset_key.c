#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "riack.h"
#include "db.h"

int main(int argc, char * argv[])
{
  riack_client * riack_client;
  riack_string bucket_type_str, bucket_str, key_str;
  riack_get_object * res_object;
  riack_object object;
   
  int curr_xid;
  char * host = "10.100.2.1";
  int port = 8087;
  char * curr_xid_str;

  riack_client = db_init(host, port);

  bucket_type_str.value = "strongly_consistent";
  bucket_type_str.len = strlen(bucket_type_str.value);

  bucket_str.value = "ids";
  bucket_str.len = strlen(bucket_str.value);

  key_str.value = "ingress_id";
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
      return -1;
    }
  }
}
