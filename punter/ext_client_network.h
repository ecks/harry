#ifndef EXT_CLIENT_NETWORK
#define EXT_CLIENT_NETWORK

#ifndef IPPROTO_OSPFIGP
#define IPPROTO_OSPFIGP	89
#endif

#define ALLSPFROUTERS6 "ff02::5"
#define ALLDROUTERS6   "ff02::6"

int ext_client_ospf6_sock_init(void);

extern void ext_client_ospf6_join_allspfrouters (struct ext_client_ospf6 * ext_client_ospf6, int ifindex);
extern void ext_client_ospf6_leave_allspfrouters (struct ext_client_ospf6 * ext_client_ospf6, u_int ifindex);
extern void ext_client_ospf6_join_alldrouters (struct ext_client_ospf6 * ext_client_ospf6, u_int ifindex);
extern void ext_client_ospf6_leave_alldrouters (struct ext_client_ospf6 * ext_client_ospf6, u_int ifindex);

extern int ext_client_ospf6_iobuf_size(unsigned int size);
extern int ext_client_ospf6_iobuf_cpy_rfp(struct rfpbuf * rfpbuf, unsigned int size);
extern void * ext_client_ospf6_iobuf_cpy_mem(void * mem, unsigned int size);

int ext_client_ospf6_sendmsg(struct in6_addr * src, struct iovec * mesage, struct ext_client_ospf6 * ext_client_ospf6);
int ext_client_ospf6_recvmsg(struct ext_client_ospf6 * ext_client_ospf6);

#endif
