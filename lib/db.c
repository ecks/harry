#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <riack.h>

#include "db.h"

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

extern struct keys * db_list_keys(struct RIACK_CLIENT * riack_client, unsigned int bucket, bool sort_req)
{
  struct RIACK_STRING_LINKED_LIST * list;
  struct RIACK_STRING_LINKED_LIST * current;
  RIACK_STRING bucket_str;
  size_t list_size;

  size_t cur_str_len;
  struct keys * keys = NULL;

  bucket_str.value = calloc(ID_SIZE, sizeof(char));

  sprintf(bucket_str.value, "%d", bucket);
  bucket_str.len = strlen(bucket_str.value);

  if(riack_list_keys(riack_client, bucket_str, &list) == RIACK_SUCCESS)
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

    riack_free_string_linked_list(riack_client, &list);
  }

  free(bucket_str.value);
  return keys;
}

extern struct keys * db_range_keys(struct RIACK_CLIENT * riack_client, unsigned int bucket, unsigned int start_xid, unsigned int end_xid, bool sort_req)
{
  RIACK_STRING bucket_str, index, min_xid, max_xid;
  RIACK_STRING_LIST list;
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

  
  if(riack_2i_query_range(riack_client, bucket_str, index, min_xid, max_xid, &list) == RIACK_SUCCESS)
  {
    list_size = list.string_count;

    keys = calloc(1, sizeof(struct keys));
    keys->num_keys = list_size;
    keys->key_str_ptrs = calloc(list_size, sizeof(char *));

    for(i = 0; i < list_size; i++)
    {
      cur_str = list.strings[i].value;
      cur_str_len = list.strings[i].len;
      keys->key_str_ptrs[i] = calloc(cur_str_len + 1, sizeof(char)); // 1 extra for null byte
      strncpy(keys->key_str_ptrs[i], cur_str, cur_str_len);
      keys->key_str_ptrs[i][cur_str_len] = '\0';
    }

    if(sort_req)
    {
      qsort(keys->key_str_ptrs, keys->num_keys, sizeof(char *), keys_compare);
    }
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
