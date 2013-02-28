#include "config.h"

#include "assert.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "stdbool.h"
#include "stddef.h"
#include "errno.h"
#include "sys/socket.h"
#include "netinet/in.h"
#include "sys/types.h"

#include "routeflow-common.h"
#include "util.h"
#include "dblist.h"
#include "prefix.h"
#include "punter_ctrl.h"
#include "ext_client.h"
#include "ext_client_network.h"

struct in6_addr allspfrouters6;
struct in6_addr alldrouters6;

unsigned int ext_client_ifindex(struct list * ipv6_addrs, struct ext_client * ext_client)
{
  struct in6_addr host_addr;
  int ifindex = -1;

  inet_pton(AF_INET6, ext_client->host, &host_addr);

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

unsigned int ext_client_mtu(struct list * ports, unsigned int ifindex)
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

int
ext_client_sock_init(void)
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

void
ext_client_join_allspfrouters (struct ext_client * ext_client, int ifindex)
{
  struct ipv6_mreq mreq6;
  int retval;
  
  assert(ifindex);
  mreq6.ipv6mr_interface = ifindex;
  memcpy (&mreq6.ipv6mr_multiaddr, &allspfrouters6,
          sizeof (struct in6_addr));

  retval = setsockopt (ext_client->sockfd, IPPROTO_IPV6, IPV6_JOIN_GROUP,
                       &mreq6, sizeof (mreq6));

 if (retval < 0)
   fprintf (stderr, "Network: Join AllSPFRouters on ifindex %d failed: %s",
             ifindex, strerror (errno));
}

void
ext_client_leave_allspfrouters (struct ext_client * ext_client, u_int ifindex)
{
  struct ipv6_mreq mreq6;

  assert (ifindex);
  mreq6.ipv6mr_interface = ifindex;
  memcpy (&mreq6.ipv6mr_multiaddr, &allspfrouters6,
          sizeof (struct in6_addr));

  if (setsockopt (ext_client->sockfd, IPPROTO_IPV6, IPV6_LEAVE_GROUP,
                  &mreq6, sizeof (mreq6)) < 0)
    fprintf (stderr, "Network: Leave AllSPFRouters on ifindex %d Failed: %s",
               ifindex, strerror (errno));
}

void
ext_client_join_alldrouters (struct ext_client * ext_client, u_int ifindex)
{
  struct ipv6_mreq mreq6;

  assert (ifindex);
  mreq6.ipv6mr_interface = ifindex;
  memcpy (&mreq6.ipv6mr_multiaddr, &alldrouters6,
          sizeof (struct in6_addr));

  if (setsockopt (ext_client->sockfd, IPPROTO_IPV6, IPV6_JOIN_GROUP,
                  &mreq6, sizeof (mreq6)) < 0)
    fprintf (stderr, "Network: Join AllDRouters on ifindex %d Failed: %s",
               ifindex, strerror (errno));
}

void
ext_client_leave_alldrouters (struct ext_client * ext_client, u_int ifindex)
{
  struct ipv6_mreq mreq6;

  assert (ifindex);
  mreq6.ipv6mr_interface = ifindex;
  memcpy (&mreq6.ipv6mr_multiaddr, &alldrouters6,
          sizeof (struct in6_addr));

  if (setsockopt (ext_client->sockfd, IPPROTO_IPV6, IPV6_LEAVE_GROUP,
                  &mreq6, sizeof (mreq6)) < 0)
    fprintf (stderr, "Network: Leave AllDRouters on ifindex %d Failed", ifindex);
}

static u_char * recvbuf = NULL;

static int iobuflen = 0;

int ext_client_iobuf_size(unsigned int size)
{
  u_char * recvnew;

  if(size <= iobuflen)
    return iobuflen;

  recvnew = calloc(1, size);

  recvbuf = recvnew;

  iobuflen = size;
  return iobuflen; 
}

int ext_client_iobuf_cpy_rfp(struct rfpbuf * rfpbuf, unsigned int size)
{
  return rfpbuf_put_init(rfpbuf, recvbuf, size);
}

void * ext_client_iobuf_cpy_mem(void * mem, unsigned int size)
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

int ext_client_recvmsg(struct ext_client * ext_client)
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

  len = recvmsg(ext_client->sockfd, &rmsghdr, 0);

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
