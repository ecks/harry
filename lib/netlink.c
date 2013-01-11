#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include "dblist.h"
#include "prefix.h"
#include "routeflow-common.h"
#include "netlink.h"

#ifndef HAVE_IPV6
#define HAVE_IPV6
#endif

/* Socket interface to kernel */
struct nlsock 
{
  int sock;
  int seq;
  struct sockaddr_nl snl;
  const char *name;
} netlink_cmd = { -1, 0, {0}, "netlink-cmd"};       /* command channel */

/* Make socket for Linux netlink interface */
static int
netlink_socket(struct nlsock * nl, unsigned long groups)
{ 
  int ret;
  struct sockaddr_nl snl;
  int sock;
  int namelen;
  int save_errno;

  sock = socket (AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
  if (sock < 0)
      return -1; 

  memset (&snl, 0, sizeof snl);
  snl.nl_family = AF_NETLINK;
  snl.nl_groups = groups;

  /* Bind the socket to the netlink structure for anything. */
  ret = bind (sock, (struct sockaddr *) &snl, sizeof snl);
  save_errno = errno;

  if (ret < 0)
    {   
      close (sock);
      return -1; 
    }   

  /* multiple netlink sockets will have different nl_pid */
  namelen = sizeof snl;
  ret = getsockname (sock, (struct sockaddr *) &snl, (socklen_t *) &namelen);
  if (ret < 0 || namelen != sizeof snl)
    {   
      close (sock);
      return -1; 
    }   

  nl->snl = snl;
  nl->sock = sock;
  return ret;
}

/* Get type specified information from netlink. */
static int
netlink_request (int family, int type, struct nlsock *nl)
{
  int ret;
  struct sockaddr_nl snl;
  int save_errno;

  struct
  {
    struct nlmsghdr nlh;
    struct rtgenmsg g;
  } req;


  /* Check netlink socket. */
  if (nl->sock < 0)
      return -1;

  memset (&snl, 0, sizeof snl);
  snl.nl_family = AF_NETLINK;

  memset (&req, 0, sizeof req);
  req.nlh.nlmsg_len = sizeof req;
  req.nlh.nlmsg_type = type;
  req.nlh.nlmsg_flags = NLM_F_ROOT | NLM_F_MATCH | NLM_F_REQUEST;
  req.nlh.nlmsg_pid = nl->snl.nl_pid;
  req.nlh.nlmsg_seq = ++nl->seq;
  req.g.rtgen_family = family;

  ret = sendto (nl->sock, (void *) &req, sizeof req, 0,
                (struct sockaddr *) &snl, sizeof snl);
  save_errno = errno;

  if (ret < 0)
      return -1;

  return 0;
}

/* Receive message from netlink interface and pass those information
 *    to the given function. */
static int
netlink_parse_info (int (*filter) (struct sockaddr_nl *, struct nlmsghdr *, void *),
                    struct nlsock *nl, void * info)
{
  int status;
  int ret = 0;
  int error;

  while (1)
  {
      char buf[4096];
      struct iovec iov = { buf, sizeof buf };
      struct sockaddr_nl snl;
      struct msghdr msg = { (void *) &snl, sizeof snl, &iov, 1, NULL, 0, 0 };
      struct nlmsghdr *h;

      status = recvmsg (nl->sock, &msg, 0);

      // Check if the socket closed
      if (nl->sock == -1)
        return -1;

      if (status < 0)
      {
        if (errno == EINTR)
          continue;
        if (errno == EWOULDBLOCK || errno == EAGAIN)
          break;
        continue;
      }
                       
      if (status == 0)
        return -1;
      
      if (msg.msg_namelen != sizeof snl)
        return -1;
      
      for (h = (struct nlmsghdr *) buf; NLMSG_OK (h, (unsigned int) status);
           h = NLMSG_NEXT (h, status))
      { 
        /* Finish of reading. */
        if (h->nlmsg_type == NLMSG_DONE)
          return ret;

       /* Error handling. */
       if (h->nlmsg_type == NLMSG_ERROR)
       {
         struct nlmsgerr *err = (struct nlmsgerr *) NLMSG_DATA (h);
         int errnum = err->error;
         int msg_type = err->msg.nlmsg_type;

         /* If the error field is zero, then this is an ACK */
         if (err->error == 0)
         {
           /* return if not a multipart message, otherwise continue */
           if (!(h->nlmsg_flags & NLM_F_MULTI))
           {
             return 0;
           }
           continue;

         }

         if (h->nlmsg_len < NLMSG_LENGTH (sizeof (struct nlmsgerr)))
           return -1;

         /* Deal with errors that occur because of races in link handling */
         if (nl == &netlink_cmd
             && ((msg_type == RTM_DELROUTE &&
                (-errnum == ENODEV || -errnum == ESRCH))
                || (msg_type == RTM_NEWROUTE && -errnum == EEXIST)))
         {
           return 0;
         }

       return -1;
     }
  
     /* OK we got netlink message. */

     /* skip unsolicited messages originating from command socket */
                                        /*
            if (nl != &netlink_cmd && h->nlmsg_pid == netlink_cmd.snl.nl_pid)
                        continue;
                                                                */
     error = (*filter) (&snl, h, info);
     if (error < 0)
       ret = error;
   }

   /* After error care. */
   if (msg.msg_flags & MSG_TRUNC)
     continue;
   if (status)
     return -1;
  }
  return ret;

}

/* Utility function for parse rtattr. */
static void
netlink_parse_rtattr (struct rtattr **tb, int max, struct rtattr *rta,
                      int len)
{
  while (RTA_OK (rta, len))
    {
      if (rta->rta_type <= max)
        tb[rta->rta_type] = rta;
      rta = RTA_NEXT (rta, len);
    }
}

/* Looking up routing table by netlink interface. */
static int
netlink_routing_table (struct sockaddr_nl *snl, struct nlmsghdr *h, void * info)
{
  // Convert info
  struct netlink_routing_table_info * real_info = (struct netlink_routing_table_info *)info;

  int len;
  struct rtmsg *rtm;
  struct rtattr *tb[RTA_MAX + 1]; 
  u_char flags = 0;

  char anyaddr[16] = { 0 };

  int index;
  int table;
  int metric;

  void *dest;
  void *gate;
  void *src;

  rtm = NLMSG_DATA (h);
    
  // Make sure this is an add or delete of a route
  if (h->nlmsg_type != RTM_NEWROUTE && h->nlmsg_type != RTM_DELROUTE)
    return 0;
 
  //if (rtm->rtm_type != RTN_UNICAST)
    //return 0;

  // Ignore unreachable routes
  if (rtm->rtm_type == RTN_UNREACHABLE)
    return 0;

  table = rtm->rtm_table;

  len = h->nlmsg_len - NLMSG_LENGTH (sizeof (struct rtmsg));
  if (len < 0)
    return -1;

  memset (tb, 0, sizeof tb);
  netlink_parse_rtattr (tb, RTA_MAX, RTM_RTA (rtm), len);

  if (rtm->rtm_flags & RTM_F_CLONED)
    return 0;
  if (rtm->rtm_protocol == RTPROT_REDIRECT)
    return 0;

  //if (rtm->rtm_protocol == RTPROT_KERNEL)
    //return 0;

  if (rtm->rtm_src_len != 0)
    return 0;

  index = 0;
  metric = 0;
  dest = NULL;
  gate = NULL;
  src = NULL;

  if (tb[RTA_OIF])
    index = *(int *) RTA_DATA (tb[RTA_OIF]);

  if (tb[RTA_DST])
    dest = RTA_DATA (tb[RTA_DST]);
  else
    dest = anyaddr;

  if (tb[RTA_PREFSRC])
    src = RTA_DATA (tb[RTA_PREFSRC]);

  /* Multipath treatment is needed. */
  if (tb[RTA_GATEWAY])
    gate = RTA_DATA (tb[RTA_GATEWAY]);

  if (tb[RTA_PRIORITY])
    metric = *(int *) RTA_DATA(tb[RTA_PRIORITY]);

  if (rtm->rtm_family == AF_INET)
  {
    // Check for callback
    if ((h->nlmsg_type == RTM_NEWROUTE && real_info->rib_add_ipv4_route) || (h->nlmsg_type == RTM_DELROUTE && real_info->rib_remove_ipv4_route))
    {
				// Construct route info
				struct route_ipv4 * route = calloc(1, sizeof(struct route_ipv4));
				if (route != NULL)
				{
					route->p = calloc(1, sizeof(struct prefix_ipv4));
					if (route->p != NULL)
					{
						route->type = 1;	// Means nothing right now
						route->flags = flags;
						route->p->family = AF_INET;
						memcpy (&route->p->prefix, dest, 4);
						route->p->prefixlen = rtm->rtm_dst_len;
						route->gate = gate;
						route->src = src;
						route->ifindex = index;
						route->vrf_id = table;
						route->metric = metric;
						route->distance = 0;
						
						// Note: Receivers responsibilty to free memory for route
						
						(h->nlmsg_type == RTM_NEWROUTE) ? real_info->rib_add_ipv4_route(route, real_info->ipv4_rib) : real_info->rib_remove_ipv4_route(route, real_info->ipv4_rib);
					}
				}
			}
    }
#ifdef HAVE_IPV6
  if (rtm->rtm_family == AF_INET6)
    {
			// Check for callback
			if ((h->nlmsg_type == RTM_NEWROUTE && real_info->rib_add_ipv6_route) || (h->nlmsg_type == RTM_DELROUTE && real_info->rib_remove_ipv6_route))
			{
				// Construct route info
				struct route_ipv6 * route = calloc(1, sizeof(struct route_ipv6));
				if (route != NULL)
				{
					route->p = calloc(1, sizeof(struct prefix_ipv6));
					if (route->p != NULL)
					{
						route->type = 1;	// Means nothing right now
						route->flags = flags;
						route->p->family = AF_INET6;
						memcpy (&route->p->prefix, dest, 16);
						route->p->prefixlen = rtm->rtm_dst_len;
						route->gate = gate;
						route->ifindex = index;
						route->vrf_id = table;
						route->metric = metric;
						route->distance = 0;
						
						// Note: Receivers responsibilty to free memory for route
						(h->nlmsg_type == RTM_NEWROUTE) ? real_info->rib_add_ipv6_route(route, real_info->ipv6_rib) : real_info->rib_remove_ipv6_route(route, real_info->ipv6_rib);
					}
				}
			}
    }
#endif /* HAVE_IPV6 */

  return 0;

}

static int
netlink_interface (struct sockaddr_nl *snl, struct nlmsghdr *h, void * info)
{
  // Convert info
  struct netlink_port_info * real_info = (struct netlink_port_info *)info;

  int len;
  struct rtattr *tb[RTA_MAX + 1];
  struct ifinfomsg * ifi;
  int index;
  unsigned int flags;
  char * name = NULL;  
  
  ifi = NLMSG_DATA (h);

  if(h->nlmsg_type != RTM_NEWLINK)
    return 0;

  len = h->nlmsg_len - NLMSG_LENGTH (sizeof (struct ifinfomsg));
  if (len < 0)
    return -1;

  memset (tb, 0, sizeof tb);
  netlink_parse_rtattr (tb, RTA_MAX, IFLA_RTA (ifi), len);

  index = ifi->ifi_index;
  flags = ifi->ifi_flags;

  if (tb[IFLA_IFNAME] == NULL)
    return -1;
  name = (char *) RTA_DATA (tb[IFLA_IFNAME]);

  if (h->nlmsg_type == RTM_NEWLINK && real_info->add_port)
  {
    real_info->add_port(index, flags, name, real_info->all_ports);
  }

  return 0;
}

/* Routing table read function using netlink interface */
int netlink_route_read(struct netlink_routing_table_info * info)
{
  int ret;
 
  /* Get IPv4 routing table */
  ret = netlink_request(AF_INET, RTM_GETROUTE, &netlink_cmd);
  if (ret < 0)
    return ret;
  ret = netlink_parse_info (netlink_routing_table, &netlink_cmd, (void*)info);
  if (ret < 0)
    return ret;

#ifdef HAVE_IPV6
  /* Get IPv6 routing table. */
  ret = netlink_request (AF_INET6, RTM_GETROUTE, &netlink_cmd);
  if (ret < 0)
    return ret;
  ret = netlink_parse_info (netlink_routing_table, &netlink_cmd, (void*)info);
  if (ret < 0)
    return ret;
#endif /* HAVE_IPV6 */

  return 0;  
}

int interface_lookup_netlink(struct netlink_port_info * info)
{
  int ret;

  ret = netlink_request(AF_PACKET, RTM_GETLINK, &netlink_cmd);
  if (ret < 0)
    return ret;
  ret = netlink_parse_info ( netlink_interface, &netlink_cmd, (void*)info);
  if (ret < 0)
    return ret;

  return 0;
}

void kernel_init(void)
{
  if(netlink_cmd.sock == -1)
    netlink_socket(&netlink_cmd, 0);
}
