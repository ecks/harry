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
#include "ext_client_provider.h"
#include "ext_client_ospf6.h"
#include "ext_client_network.h"

#define BUFSIZE 1500

enum ext_client_ospf6_state
{
  EXT_CONNECTING
};

enum event {EXT_CLIENT_SCHEDULE, EXT_CLIENT_READ};

static void ext_client_ospf6_event(enum event event, struct ext_client_ospf6 * ext_client_ospf6);

extern struct thread_master * master;

struct ext_client_ospf6 * ext_client_ospf6_new()
{
  return calloc(1, sizeof(struct ext_client_ospf6));
}

void ext_client_ospf6_init(struct ext_client_ospf6 * ext_client_ospf6, char * host, struct punter_ctrl * punter_ctrl)
{
  ext_client_ospf6->sockfd = -1;
  ext_client_ospf6->host = host;
  ext_client_ospf6->punter_ctrl = punter_ctrl;
  ext_client_ospf6->state = EXT_CONNECTING;

  ext_client_ospf6->addr = calloc(1, sizeof(struct addrinfo));

  // find the ifindex corresponding to the ip aaddress
  ext_client_ospf6->ifindex = ext_client_ospf6_ifindex(ext_client_ospf6->punter_ctrl->ipv6_addrs, ext_client_ospf6);

  // set up the linklocal addr
  ext_client_ospf6->linklocal = calloc(1, sizeof(struct in6_addr));
  inet_pton(AF_INET6, host, ext_client_ospf6->linklocal);

  // find the mtu
  ext_client_ospf6->mtu = ext_client_ospf6_mtu(ext_client_ospf6->punter_ctrl->port_list, ext_client_ospf6->ifindex);
  ext_client_ospf6_iobuf_size(ext_client_ospf6->mtu);

//  ext_client->ibuf = rfpbuf_new(ext_client->mtu);
  ext_client_ospf6->ibuf = NULL;

  ext_client_ospf6_event(EXT_CLIENT_SCHEDULE, ext_client_ospf6);
}

static void ext_client_ospf6_init2(struct ext_client * ext_client)
{

}

static void ext_client_ospf6_send2(struct ext_client * ext_client)
{

}

static int ext_client_ospf6_start(struct ext_client_ospf6 * ext_client_ospf6)
{
  int retval;

  if(ext_client_ospf6->sockfd >= 0)
    return 0;
 
  if(ext_client_ospf6->t_connect)
    return 0;

  if((ext_client_ospf6->sockfd = ext_client_ospf6_sock_init()) < 0)
  {
    printf("ext_client connection fail\n");
    return -1;
  }

  printf("ifindex: %d\n", ext_client_ospf6->ifindex);

  ext_client_ospf6_join_allspfrouters(ext_client_ospf6, ext_client_ospf6->ifindex );
  ext_client_ospf6_join_alldrouters(ext_client_ospf6, ext_client_ospf6->ifindex );

//  if((ext_client->ibuf = rfpbuf_new(60 * 1024)) == NULL)
//  {
//    fprintf(stderr, "rfpbuf_new failed");
//  }

  ext_client_ospf6->addr = calloc(1, sizeof(struct addrinfo));

  ext_client_ospf6_event(EXT_CLIENT_READ, ext_client_ospf6);

  return 0; 
}

static int ext_client_ospf6_connect(struct thread * t)
{
  struct ext_client_ospf6 * ext_client_ospf6 = NULL;
 
  ext_client_ospf6 = THREAD_ARG(t);
  ext_client_ospf6->t_connect = NULL;

  return ext_client_ospf6_start(ext_client_ospf6); 
}

int ext_client_ospf6_send(struct rfpbuf * buf, struct ext_client_ospf6 * ext_client_ospf6)
{
  struct iovec iovector[2];
  struct ospf6_header * oh;

  oh = rfpbuf_at_assert(buf, 0, sizeof(struct ospf6_header));

  iovector[0].iov_base = (caddr_t)oh;
  iovector[0].iov_len = ntohs(oh->length);
  iovector[1].iov_base = NULL;
  iovector[1].iov_len = 0;


  ext_client_ospf6_sendmsg(ext_client_ospf6->linklocal, iovector, ext_client_ospf6); // src needs to be the linclocal address, leave blank for now
  return 0;
}

static int ext_client_ospf6_read(struct thread * t)
{
  printf("read triggered!\n");

  struct ext_client_ospf6 * ext_client_ospf6 = THREAD_ARG(t);
  struct rfp_forward_ospf6 * rfp6;
  char buf[BUFSIZE];
  struct msghdr msgh;
  struct ip * ip;
  struct icmp * icmp;
  int ip_hdr_len;
  int icmp_hdr_len;
  int nbytes;

  ext_client_ospf6_event(EXT_CLIENT_READ, ext_client_ospf6);

  nbytes = ext_client_ospf6_recvmsg(ext_client_ospf6);
  if(nbytes < 0)
  {
    perror("ext_client_ospf6_recvmsg error");
  }

  // put a header
  ext_client_ospf6->ibuf = routeflow_alloc_xid(RFPT_FORWARD_OSPF6, RFP10_VERSION, 0, sizeof(struct rfp_forward_ospf6));

  rfp6 = ext_client_ospf6->ibuf->l2;

  // copy data over
  ext_client_ospf6_iobuf_cpy_mem(&rfp6->ospf6_header, sizeof(struct rfp_header)); // put in ospf6 header only -- for now

  punter_ext_to_zl_forward_msg(); // punt the message over to zebralite

  // we have sent the data, so free the memory
  rfpbuf_delete(ext_client_ospf6->ibuf);
  ext_client_ospf6->ibuf = 0;

  return 0;
}

static void ext_client_ospf6_event(enum event event, struct ext_client_ospf6 * ext_client_ospf6)
{
  switch(event)
  { 
    case EXT_CLIENT_SCHEDULE:
      ext_client_ospf6->t_connect = thread_add_event(master, ext_client_ospf6_connect, ext_client_ospf6, 0);
      break;

    case EXT_CLIENT_READ:
      ext_client_ospf6->t_read = thread_add_read(master, ext_client_ospf6_read, ext_client_ospf6, ext_client_ospf6->sockfd);
      break;

    default:
      break;
  }
}

#define EXT_CLIENT_INIT                   \
  {                                       \
    ext_client_ospf6_init2,               \
    ext_client_ospf6_send2,               \
  }

struct ext_client_class ospf6_ext_client_class = EXT_CLIENT_INIT;
