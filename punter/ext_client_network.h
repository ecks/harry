#ifndef EXT_CLIENT_NETWORK
#define EXT_CLIENT_NETWORK

#ifndef IPPROTO_OSPFIGP
#define IPPROTO_OSPFIGP	89
#endif

#define ALLSPFROUTERS6 "ff02::5"
#define ALLDROUTERS6   "ff02::6"

/* Type */
#define OSPF6_MESSAGE_TYPE_UNKNOWN  0x0
#define OSPF6_MESSAGE_TYPE_HELLO    0x1  /* Discover/maintain neighbors */
#define OSPF6_MESSAGE_TYPE_DBDESC   0x2  /* Summarize database contents */
#define OSPF6_MESSAGE_TYPE_LSREQ    0x3  /* Database download request */
#define OSPF6_MESSAGE_TYPE_LSUPDATE 0x4  /* Database update */
#define OSPF6_MESSAGE_TYPE_LSACK    0x5  /* Flooding acknowledgment */
#define OSPF6_MESSAGE_TYPE_ALL      0x6  /* For debug option */

/* OSPFv3 packet header */
struct ospf6_header
{
  u_char    version;
  u_char    type;
  u_int16_t length;
  u_int32_t router_id;
  u_int32_t area_id;
  u_int16_t checksum;
  u_char    instance_id;
  u_char    reserved;
};


int ext_client_sock_init(void);

extern void ext_client_join_allspfrouters (struct ext_client * ext_client, int ifindex);
extern void ext_client_leave_allspfrouters (struct ext_client * ext_client, u_int ifindex);
extern void ext_client_join_alldrouters (struct ext_client * ext_client, u_int ifindex);
extern void ext_client_leave_alldrouters (struct ext_client * ext_client, u_int ifindex);

extern int ext_client_iobuf_size(unsigned int size);
extern int ext_client_iobuf_cpy(struct rfpbuf * rfpbuf, unsigned int size);

int ext_client_recvmsg(struct ext_client * ext_client);

#endif
