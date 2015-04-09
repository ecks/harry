#ifndef DB_H
#define DB_H

#define ID_SIZE 30

#define INDEX_KEY   "index1_int"

struct keys
{
  char ** key_str_ptrs;
  size_t num_keys;
};

extern struct keys * db_list_keys(struct RIACK_CLIENT * riack_client, unsigned int bucket, bool sort_req);
extern struct keys * db_range_keys(struct RIACK_CLIENT * riack_client, unsigned int bucket, unsigned int start_xid, unsigned int end_xid, bool sort_req);
extern void db_free_keys(struct keys * keys);
#endif
