#ifndef DB_H
#define DB_H

#define ID_SIZE 10

struct keys
{
  char ** key_str_ptrs;
  size_t num_keys;
};

extern struct keys * db_list_keys(struct RIACK_CLIENT * riack_client, unsigned int bucket, bool sort_req);

#endif
