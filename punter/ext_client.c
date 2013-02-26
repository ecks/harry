#include "config.h"

#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "stdbool.h"
#include "netdb.h"
#include "netinet/in.h"
#include "netinet/ip.h"
#include "netinet/ip_icmp.h"
#include "sys/socket.h"

#include "routeflow-common.h"
#include "dblist.h"
#include "rfpbuf.h"
#include "rfp-msgs.h"
#include "thread.h"
#include "punter_ctrl.h"
#include "ext_client.h"
#include "ext_client_network.h"

#define BUFSIZE 1500

enum ext_client_state
{
  EXT_CONNECTING
};

enum event {EXT_CLIENT_SCHEDULE, EXT_CLIENT_READ};

static void ext_client_event(enum event event, struct ext_client * ext_client);

extern struct thread_master * master;

struct ext_client * ext_client_new()
{
  return calloc(1, sizeof(struct ext_client));
}

void ext_client_init(struct ext_client * ext_client, char * host, struct punter_ctrl * punter_ctrl)
{
  ext_client->sockfd = -1;
  ext_client->host = host;
  ext_client->punter_ctrl = punter_ctrl;
  ext_client->state = EXT_CONNECTING;

  ext_client->addr = calloc(1, sizeof(struct addrinfo));

  // find the ifindex corresponding to the ip aaddress
  ext_client->ifindex = ext_client_ifindex(ext_client->punter_ctrl->ipv6_addrs, ext_client);

  // find the mtu
  ext_client->mtu = ext_client_mtu(ext_client->punter_ctrl->port_list, ext_client->ifindex);
  ext_client_iobuf_size(ext_client->mtu);

  ext_client->ibuf = rfpbuf_new(ext_client->mtu);
  rfpbuf_init(ext_client->ibuf, ext_client->mtu);

  ext_client_event(EXT_CLIENT_SCHEDULE, ext_client);
}

static int ext_client_start(struct ext_client * ext_client)
{
  int retval;

  if(ext_client->sockfd >= 0)
    return 0;
 
  if(ext_client->t_connect)
    return 0;

  if((ext_client->sockfd = ext_client_sock_init()) < 0)
  {
    printf("ext_client connection fail\n");
    return -1;
  }

  printf("ifindex: %d\n", ext_client->ifindex);

  ext_client_join_allspfrouters(ext_client, ext_client->ifindex );
  ext_client_join_alldrouters(ext_client, ext_client->ifindex );

  if((ext_client->ibuf = rfpbuf_new(60 * 1024)) == NULL)
  {
    fprintf(stderr, "rfpbuf_new failed");
  }

  ext_client->addr = calloc(1, sizeof(struct addrinfo));

  ext_client_event(EXT_CLIENT_READ, ext_client);

  return 0; 
}

static int ext_client_connect(struct thread * t)
{
  struct ext_client * ext_client = NULL;
 
  ext_client = THREAD_ARG(t);
  ext_client->t_connect = NULL;

  return ext_client_start(ext_client); 
}

static int ext_client_read(struct thread * t)
{
  printf("read triggered!\n");

  struct ext_client * ext_client = THREAD_ARG(t);
  char buf[BUFSIZE];
  struct msghdr msgh;
  struct ip * ip;
  struct icmp * icmp;
  int ip_hdr_len;
  int icmp_hdr_len;
  int nbytes;

  ext_client_event(EXT_CLIENT_READ, ext_client);

  nbytes = ext_client_recvmsg(ext_client);
  if(nbytes < 0)
  {
    perror("ext_client_recvmsg error");
  }

  // copy data over
  ext_client_iobuf_cpy(ext_client->ibuf, nbytes); 

  // TODO: put a header
  struct rfpbuf * buffer = routeflow_alloc_xid(RFPT_FORWARD_OSPF6, RFP10_VERSION, 0, sizeof(struct rfp_forward_ospf6));
  rfpbuf_put_init(buffer, ext_client->ibuf->data, sizeof(struct rfp_header)); // put in ospf6 header only -- for now
 
  rfpbuf_delete(ext_client->ibuf);

  // reset the pointer
  ext_client->ibuf = buffer;


  punter_forward_msg(); // punt the message over to zebralite

  return 0;
}

static void ext_client_event(enum event event, struct ext_client * ext_client)
{
  switch(event)
  { 
    case EXT_CLIENT_SCHEDULE:
      ext_client->t_connect = thread_add_event(master, ext_client_connect, ext_client, 0);
      break;

    case EXT_CLIENT_READ:
      ext_client->t_read = thread_add_read(master, ext_client_read, ext_client, ext_client->sockfd);
      break;

    default:
      break;
  }
}
