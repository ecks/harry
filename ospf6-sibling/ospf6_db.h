#ifndef OSPF6_DB_H
#define OSPF6_DB_H

#define ID_SIZE 10
#define DATA_SIZE 200

extern int ospf6_db_put_hello(struct ospf6_header * oh, unsigned int xid);
extern int ospf6_db_fill(struct RIACK_CLIENT * riack_client, struct ospf6_header * oh, unsigned int xid, unsigned int bucket);
extern int ospf6_db_get(struct ospf6_header * oh, unsigned int xid, unsigned int bucket);
extern int ospf6_db_delete(unsigned int xid, unsigned int bucket);

#endif
