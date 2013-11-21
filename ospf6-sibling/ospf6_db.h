#ifndef OSPF6_DB_H
#define OSPF6_DB_H

extern int ospf6_db_put_hello(struct ospf6_header * oh, unsigned int xid);
extern struct ospf6_header * ospf6_db_get(struct ospf6_header * oh, unsigned int xid, unsigned int bucket);

#endif
