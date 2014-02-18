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
  
  return strcmp(a_str, b_str);
}

extern struct keys * db_list_keys(struct RIACK_CLIENT * riack_client, unsigned int bucket, bool sort_req)
{
  struct RIACK_STRING_LINKED_LIST * list;
  struct RIACK_STRING_LINKED_LIST * current;
  RIACK_STRING bucket_str;
  size_t list_size;

  size_t cur_str_len;
  struct keys * keys = NULL;
  struct key * key = NULL;

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
