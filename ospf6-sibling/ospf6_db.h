#ifndef OSPF6_DB_H
#define OSPF6_DB_H

#define DATA_SIZE 200

#define INDEX_KEY "index1_int"

extern int ospf6_db_put_hello(struct ospf6_header * oh, unsigned int xid);
extern int ospf6_db_put_dbdesc(struct ospf6_header * oh, unsigned int xid);
extern char * ospf6_db_get(struct ospf6_header * oh, unsigned int xid, unsigned int bucket); // fills header
extern int ospf6_db_fill_body(char * buffer, void * body); // fills the body
extern struct keys * ospf6_db_list_keys(unsigned int bucket);
extern struct keys * ospf6_db_range_keys(unsigned int bucket, unsigned int start_xid, unsigned int end_xid);
extern int ospf6_db_delete(unsigned int xid, unsigned int bucket);

#endif
