#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <riack.h>

#include "db.h"

struct option longopts[] =
{
  { "bucket", required_argument, NULL, 'b'},
  { 0 } 
};

int main(int argc, char * argv[])
{
  int opt;
  unsigned int bucket;
  char * host = "10.100.2.1";
  int port = 8087;
  int i;
  riack_client * riack_client;

  riack_string key, bucket_str;

  struct keys * keys;

  while(1)
  {
    opt = getopt_long(argc, argv, "b:", longopts, 0);

    if(opt == EOF)
      break;

    switch(opt)
    {
      case 'b':
        bucket = (unsigned int)strtol(optarg, NULL, 10);
        break;

    }
  }

  riack_init();
  riack_client = riack_new_client(0);
  if(riack_connect(riack_client, host, port, 0) != RIACK_SUCCESS)
  {
    // give up
    exit(0);
  }

  keys = db_list_keys(riack_client, bucket, false);


  for(i = 0; i < keys->num_keys; i++)
  {
    key.value = calloc(ID_SIZE, sizeof(char));
    bucket_str.value = calloc(ID_SIZE, sizeof(char));

    sprintf(bucket_str.value, "%d", bucket);
    bucket_str.len = strlen(bucket_str.value);

    sprintf(key.value, "%s", keys->key_str_ptrs[i]);
    key.len = strlen(key.value);

    // delete the object from riack database
    if(riack_delete(riack_client, &bucket_str, &key, 0) == RIACK_SUCCESS)
    {
       
    }

    free(key.value);
    free(bucket_str.value);
  }
}
