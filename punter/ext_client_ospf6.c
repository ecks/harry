#include "config.h"

#include "stdlib.h"
#include "assert.h"
#include "stdio.h"
#include "string.h"
#include "stdbool.h"
#include "stddef.h"
#include "errno.h"
#include "netdb.h"
#include "netinet/in.h"
#include "netinet/ip.h"
#include "netinet/ip_icmp.h"
#include "sys/socket.h"
#include "sys/types.h"

#include "routeflow-common.h"
#include "util.h"
#include "dblist.h"
#include "rfpbuf.h"
#include "rfp-msgs.h"
#include "prefix.h"
#include "thread.h"
#include "punter_ctrl.h"
#include "ext_client_provider.h"

#define BUFSIZE 1500

#ifndef IPPROTO_OSPFIGP
#define IPPROTO_OSPFIGP	89
#endif

#define ALLSPFROUTERS6 "ff02::5"
#define ALLDROUTERS6   "ff02::6"

struct in6_addr allspfrouters6;
struct in6_addr alldrouters6;

struct ext_client_ospf6
{
  struct ext_client ext_client;
  struct in6_addr * linklocal;
  unsigned int mtu;

  struct thread * t_connect;
  struct thread * t_read;

  struct addrinfo * addr;

  int state;
};

enum ext_client_ospf6_state
{
  EXT_CONNECTING
};

enum event {EXT_CLIENT_SCHEDULE, EXT_CLIENT_READ};

static void ext_client_ospf6_event(enum event event, struct ext_client_ospf6 * ext_client_ospf6);

extern struct thread_master * master;

unsigned int ext_client_ospf6_ifindex(struct list * ipv6_addrs, struct ext_client_ospf6 * ext_client_ospf6)
{
  struct in6_addr host_addr;
  int ifindex = -1;

  inet_pton(AF_INET6, P(ext_client_ospf6).host, &host_addr);

  struct addr_ipv6 * addr;
  LIST_FOR_EACH(addr, struct addr_ipv6, node, ipv6_addrs)
  {
    if(memcmp(addr->p->prefix.s6_addr, host_addr.s6_addr, 16) == 0) // compare 16 bytes
    {
      ifindex = addr->ifindex;
    }
  }

  return ifindex;
}

static unsigned int ext_client_ospf6_mtu(struct list * ports, unsigned int ifindex)
{
  unsigned int mtu;

  struct sw_port * port;
  LIST_FOR_EACH(port, struct sw_port, node, ports)
  {
    if(port->port_no == ifindex)
    {
      mtu = port->mtu;
    }
  }
 
  return mtu;
}

static int ext_client_ospf6_sock_init(void)
{
  int sock;

//  if ( svd_privs.change (ZPRIVS_RAISE) )
//    zlog_err ("shim_sock_init: could not raise privs, %s",
//               safe_strerror (errno) );
    
  sock = socket (AF_INET6, SOCK_RAW, IPPROTO_OSPFIGP);
  if (sock < 0)
  {   
      int save_errno = errno;
//      if ( svd_privs.change (ZPRIVS_LOWER) )
//        zlog_err ("shim_sock_init: could not lower privs, %s",
//                   safe_strerror (errno) );
      fprintf (stderr, "shim_sock_init: socket: %s", strerror (save_errno));
      exit(1);
  }   
    
//  if (svd_privs.change (ZPRIVS_LOWER))
//    {   
//      zlog_err ("shim_sock_init: could not lower privs, %s",
//               safe_strerror (errno) );
//    }   

  sockopt_reuseaddr (sock);

  u_int off = 0;
  if (setsockopt (sock, IPPROTO_IPV6, IPV6_MULTICAST_LOOP,
                  &off, sizeof (u_int)) < 0)
    fprintf (stderr, "Network: reset IPV6_MULTICAST_LOOP failed: %s",
               strerror (errno));

  if (setsockopt_ifindex (AF_INET6, sock, 0) < 0)
     fprintf (stderr, "Can't set pktinfo option for fd %d", sock);

  int offset = 12; 
#ifndef DISABLE_IPV6_CHECKSUM
  if (setsockopt (sock, IPPROTO_IPV6, IPV6_CHECKSUM,
                  &offset, sizeof (offset)) < 0)
    fprintf (stderr, "Network: set IPV6_CHECKSUM failed: %s", strerror (errno));
#else
  fprintf (stderr, "Network: Don't set IPV6_CHECKSUM");
#endif /* DISABLE_IPV6_CHECKSUM */

  inet_pton (AF_INET6, ALLSPFROUTERS6, &allspfrouters6);
  inet_pton (AF_INET6, ALLDROUTERS6, &alldrouters6);

  return sock;
}


static void ext_client_ospf6_join_allspfrouters (struct ext_client_ospf6 * ext_client_ospf6, unsigned int ifindex)
{
  struct ipv6_mreq mreq6;
  int retval;
  
  assert(ifindex);
  mreq6.ipv6mr_interface = ifindex;
  memcpy (&mreq6.ipv6mr_multiaddr, &allspfrouters6,
          sizeof (struct in6_addr));

  retval = setsockopt (P(ext_client_ospf6).sockfd, IPPROTO_IPV6, IPV6_JOIN_GROUP,
                       &mreq6, sizeof (mreq6));

 if (retval < 0)
   fprintf (stderr, "Network: Join AllSPFRouters on ifindex %d failed: %s",
             ifindex, strerror (errno));
}

static void ext_client_ospf6_leave_allspfrouters (struct ext_client_ospf6 * ext_client_ospf6, u_int ifindex)
{
  struct ipv6_mreq mreq6;

  assert (ifindex);
  mreq6.ipv6mr_interface = ifindex;
  memcpy (&mreq6.ipv6mr_multiaddr, &allspfrouters6,
          sizeof (struct in6_addr));

  if (setsockopt (P(ext_client_ospf6).sockfd, IPPROTO_IPV6, IPV6_LEAVE_GROUP,
                  &mreq6, sizeof (mreq6)) < 0)
    fprintf (stderr, "Network: Leave AllSPFRouters on ifindex %d Failed: %s",
               ifindex, strerror (errno));
}

static void ext_client_ospf6_join_alldrouters (struct ext_client_ospf6 * ext_client_ospf6, u_int ifindex)
{
  struct ipv6_mreq mreq6;

  assert (ifindex);
  mreq6.ipv6mr_interface = ifindex;
  memcpy (&mreq6.ipv6mr_multiaddr, &alldrouters6,
          sizeof (struct in6_addr));

  if (setsockopt (P(ext_client_ospf6).sockfd, IPPROTO_IPV6, IPV6_JOIN_GROUP,
                  &mreq6, sizeof (mreq6)) < 0)
    fprintf (stderr, "Network: Join AllDRouters on ifindex %d Failed: %s",
               ifindex, strerror (errno));
}

static void ext_client_ospf6_leave_alldrouters (struct ext_client_ospf6 * ext_client_ospf6, u_int ifindex)
{
  struct ipv6_mreq mreq6;

  assert (ifindex);
  mreq6.ipv6mr_interface = ifindex;
  memcpy (&mreq6.ipv6mr_multiaddr, &alldrouters6,
          sizeof (struct in6_addr));

  if (setsockopt (P(ext_client_ospf6).sockfd, IPPROTO_IPV6, IPV6_LEAVE_GROUP,
                  &mreq6, sizeof (mreq6)) < 0)
    fprintf (stderr, "Network: Leave AllDRouters on ifindex %d Failed", ifindex);
}

static u_char * recvbuf = NULL;

static int iobuflen = 0;

static int ext_client_ospf6_iobuf_size(unsigned int size)
{
  u_char * recvnew;

  if(size <= iobuflen)
    return iobuflen;

  recvnew = calloc(1, size);

  recvbuf = recvnew;

  iobuflen = size;
  return iobuflen; 
}

//int ext_client_ospf6_iobuf_cpy_rfp(struct rfpbuf * rfpbuf, unsigned int size)
//{
//  return rfpbuf_put(rfpbuf, recvbuf, size);
//}

static void * ext_client_ospf6_iobuf_cpy_mem(void * mem, unsigned int size)
{
  return memcpy(mem, recvbuf, size);
}

static int 
iov_count (struct iovec *iov)
{
  int i;
  for (i = 0; iov[i].iov_base; i++)
    ;   
  return i;
}

static int ext_client_ospf6_sendmsg(struct in6_addr * src, struct iovec * message, struct ext_client_ospf6 * ext_client_ospf6)
{
  int retval;
  struct msghdr smsghdr;

  struct cmsghdr *scmsgp;
  u_char cmsgbuf[CMSG_SPACE(sizeof(struct in6_pktinfo))];

  struct in6_pktinfo *pktinfo;
  struct sockaddr_in6 dst_sin6;

  scmsgp = (struct cmsghdr *)cmsgbuf;
  pktinfo = (struct in6_pktinfo *)(CMSG_DATA(scmsgp));

  /* source address */
  pktinfo->ipi6_ifindex = P(ext_client_ospf6).ifindex;
  if (src)
    memcpy (&pktinfo->ipi6_addr, src, sizeof (struct in6_addr));
  else
    memset (&pktinfo->ipi6_addr, 0, sizeof (struct in6_addr));

  memset (&dst_sin6, 0, sizeof (struct sockaddr_in6));

  /* destination address */
  dst_sin6.sin6_family = AF_INET6;
#ifdef SIN6_LEN
  dst_sin6.sin6_len = sizeof (struct sockaddr_in6);
#endif
  memcpy (&dst_sin6.sin6_addr, &allspfrouters6, sizeof (struct in6_addr));
#ifdef HAVE_SIN6_SCOPE_ID
  dst_sin6.sin6_scope_id = P(ext_client_ospf6)->ifindex;
#endif

  /* send control msg */
  scmsgp->cmsg_level = IPPROTO_IPV6;
  scmsgp->cmsg_type = IPV6_PKTINFO;
  scmsgp->cmsg_len = CMSG_LEN (sizeof (struct in6_pktinfo));

  memset(&smsghdr, 0, sizeof(struct msghdr));
  smsghdr.msg_iov =  message;
  smsghdr.msg_iovlen = iov_count(message);
  smsghdr.msg_name = (caddr_t) &dst_sin6;
  smsghdr.msg_namelen = sizeof (struct sockaddr_in6);
  smsghdr.msg_control = (caddr_t)cmsgbuf;
  smsghdr.msg_controllen = sizeof(cmsgbuf);

  if((retval = sendmsg(P(ext_client_ospf6).sockfd, &smsghdr, 0)) < 0)
  {
    printf("sendmsg failed: %s (%d)\n", strerror(errno), errno);
    perror("sendmsg");
  }

  return retval;
}

static int ext_client_ospf6_recvmsg(struct ext_client_ospf6 * ext_client_ospf6)
{
  struct msghdr rmsghdr;
  struct iovec iovector[2];
  u_char cmsgbuf[CMSG_SPACE(sizeof(struct in6_pktinfo))];
  struct ospf6_header * oh;
  int len;

  memset(recvbuf, 0, iobuflen);
  iovector[0].iov_base = recvbuf;
  iovector[0].iov_len = iobuflen;
  iovector[1].iov_base = NULL;
  iovector[1].iov_len = 0;

  
  memset(&rmsghdr, 0, sizeof(struct msghdr));
  rmsghdr.msg_iov = iovector;
  rmsghdr.msg_iovlen = iov_count(iovector);
  rmsghdr.msg_control = (caddr_t) cmsgbuf;
  rmsghdr.msg_controllen = sizeof(cmsgbuf);

  len = recvmsg(P(ext_client_ospf6).sockfd, &rmsghdr, 0);

  printf("length: %d\n", len);
  if(len > iobuflen)
  {
    fprintf(stderr, "Message too long\n");
  }
  else if(len < sizeof(struct ospf6_header))
  {
    fprintf(stderr, "Message too short\n");
  }

  oh = (struct ospf6_header *)recvbuf;

  switch(oh->type)
  {
    case OSPF6_MESSAGE_TYPE_HELLO:
      printf("Hello received\n");
      break;

    default:
      break;
  }

  return len;
}


static struct ext_client_ospf6 * ext_client_ospf6_new()
{
  return calloc(1, sizeof(struct ext_client_ospf6));
}

static struct ext_client_ospf6 * ext_client_ospf6_cast(struct ext_client * ext_client)
{
  return CONTAINER_OF(ext_client, struct ext_client_ospf6, ext_client);
}

struct ext_client * ext_client_ospf6_init(char * host, struct punter_ctrl * punter_ctrl)
{
  struct ext_client_ospf6 * ext_client_ospf6 = ext_client_ospf6_new();

  P(ext_client_ospf6).host = host;
  P(ext_client_ospf6).punter_ctrl = punter_ctrl;
  P(ext_client_ospf6).sockfd = -1;
  P(ext_client_ospf6).ibuf = NULL;
  P(ext_client_ospf6).obuf = NULL;

  ext_client_ospf6->state = EXT_CONNECTING;
  ext_client_ospf6->addr = calloc(1, sizeof(struct addrinfo));

  // find the ifindex corresponding to the ip aaddress
  P(ext_client_ospf6).ifindex = ext_client_ospf6_ifindex(P(ext_client_ospf6).punter_ctrl->ipv6_addrs, ext_client_ospf6);

  // set up the linklocal addr
  ext_client_ospf6->linklocal = calloc(1, sizeof(struct in6_addr));
  inet_pton(AF_INET6, P(ext_client_ospf6).host, ext_client_ospf6->linklocal);

  // find the mtu
  ext_client_ospf6->mtu = ext_client_ospf6_mtu(P(ext_client_ospf6).punter_ctrl->port_list, P(ext_client_ospf6).ifindex);
  ext_client_ospf6_iobuf_size(ext_client_ospf6->mtu);

  ext_client_ospf6_event(EXT_CLIENT_SCHEDULE, ext_client_ospf6);

  return &ext_client_ospf6->ext_client;
}

static int ext_client_ospf6_start(struct ext_client_ospf6 * ext_client_ospf6)
{
  int retval;
  struct ext_client * ext_client = &P(ext_client_ospf6);

  if(ext_client->sockfd >= 0)
    return 0;
 
  if(ext_client_ospf6->t_connect)
    return 0;

  if((P(ext_client_ospf6).sockfd = ext_client_ospf6_sock_init()) < 0)
  {
    printf("ext_client connection fail\n");
    return -1;
  }

  printf("ifindex: %d\n", ext_client->ifindex);

  ext_client_ospf6_join_allspfrouters(ext_client_ospf6, P(ext_client_ospf6).ifindex );
  ext_client_ospf6_join_alldrouters(ext_client_ospf6, P(ext_client_ospf6).ifindex );

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

void ext_client_ospf6_send(struct ext_client * ext_client)
{
  struct ext_client_ospf6 * ext_client_ospf6 = ext_client_ospf6_cast(ext_client);
  struct iovec iovector[2];
  struct ospf6_header * oh;

  oh = rfpbuf_at_assert(ext_client->obuf, 0, sizeof(struct ospf6_header));

  iovector[0].iov_base = (caddr_t)oh;
  iovector[0].iov_len = ntohs(oh->length);
  iovector[1].iov_base = NULL;
  iovector[1].iov_len = 0;


  ext_client_ospf6_sendmsg(ext_client_ospf6->linklocal, iovector, ext_client_ospf6); // src needs to be the linclocal address, leave blank for now
}

static int ext_client_ospf6_recv(struct thread * t)
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
  P(ext_client_ospf6).ibuf = routeflow_alloc_xid(RFPT_FORWARD_OSPF6, RFP10_VERSION, 0, sizeof(struct rfp_forward_ospf6));

  rfp6 = P(ext_client_ospf6).ibuf->l2;

  // copy data over
  ext_client_ospf6_iobuf_cpy_mem(&rfp6->ospf6_header, sizeof(struct rfp_header)); // put in ospf6 header only -- for now

  // call our parent's class
  ext_client_recv(&P(ext_client_ospf6));

  // we have sent the data, so free the memory
  rfpbuf_delete(P(ext_client_ospf6).ibuf);
  P(ext_client_ospf6).ibuf = 0;

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
      ext_client_ospf6->t_read = thread_add_read(master, ext_client_ospf6_recv, ext_client_ospf6, P(ext_client_ospf6).sockfd);
      break;

    default:
      break;
  }
}

#define EXT_CLIENT_INIT                   \
  {                                       \
    ext_client_ospf6_init,                \
    ext_client_ospf6_send,                \
  }

struct ext_client_class ospf6_ext_client_class = EXT_CLIENT_INIT;
