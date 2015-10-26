#include "config.h"

#include "stdio.h"
#include "stdlib.h"
#include "stdint.h"
#include "stddef.h"
#include "string.h"
#include <stdbool.h>
#include "pthread.h"
#include "netdb.h"
#include "netinet/in.h"
#include "sys/socket.h"

#include "util.h"
#include "dblist.h"
#include "prefix.h"
#include "routeflow-common.h"
#include "rfpbuf.h"
#include "rfp-msgs.h"
#include "if.h"
#include "debug.h"
#include "thread.h"
#include "riack.h"
#include "ospf6_top.h"
#include "ospf6_route.h"
#include "ospf6_interface.h"
#include "ospf6_restart.h"
#include "sibling_ctrl.h"
#include "ctrl_client.h"
#include "ospf6_message.h"

enum event {CTRL_CLIENT_SCHEDULE, CTRL_CLIENT_CONNECT, CTRL_CLIENT_READ, CTRL_CLIENT_CONNECTED};

/* Prototype for event manager */
static void ctrl_client_event(enum event, struct ctrl_client * ctrl_client);

extern struct thread_master * master;

struct ctrl_client * ctrl_client_new()
{
  struct ctrl_client * ctrl_client = calloc(1, sizeof(*ctrl_client));
  ctrl_client->if_list = if_init();

  return ctrl_client;
}

void ctrl_client_init(struct ctrl_client * ctrl_client, 
                      struct in6_addr * ctrl_addr, 
                      struct in6_addr * sibling_addr)
{
  ctrl_client->sock = -1;

  ctrl_client->ctrl_addr = ctrl_addr;
  ctrl_client->sibling_addr = sibling_addr;
  ctrl_client->current_xid = 0;
  ctrl_client->state = CTRL_CONNECTING;

  ctrl_client_event(CTRL_CLIENT_SCHEDULE, ctrl_client);
}

void ctrl_client_state_transition(struct ctrl_client * ctrl_client, enum ctrl_client_state new_state)
{
  if(new_state == CTRL_INTERFACE_UP)
  {
    if(ctrl_client->state == CTRL_RCVD_HELLO)
    {
      ctrl_client->state = new_state;
      sibling_ctrl_update_state(SCG_CTRL_INT_UP_REQ);
    }
    else if(ctrl_client->state == CTRL_LEAD_ELECT_RCVD)
    {
      ctrl_client->state = CTRL_CONNECTED;
      sibling_ctrl_update_state(SCG_CTRL_INT_UP_REQ);
    }
    else
    {
      if(IS_OSPF6_SIBLING_DEBUG_CTRL_CLIENT)
      {
        zlog_debug("Entered a bad state");
        return;
//        exit(1);
      }
    }
  }
  else if(new_state == CTRL_LEAD_ELECT_RCVD)
  {
    if(ctrl_client->state == CTRL_RCVD_HELLO)
    {
      ctrl_client->state = new_state;
      sibling_ctrl_update_state(SCG_CTRL_LEAD_ELECT_RCVD_REQ);
    }
    else if(ctrl_client->state == CTRL_INTERFACE_UP)
    {
      ctrl_client->state = CTRL_CONNECTED;
      sibling_ctrl_update_state(SCG_CTRL_LEAD_ELECT_RCVD_REQ);
    }
    else if(ctrl_client->state == CTRL_CONNECTED)
    {
      // leader election can happen for a restarted sibling
      // in this case, we redo leader election even though we have done it previously
      sibling_ctrl_update_state(SCG_CTRL_LEAD_ELECT_RCVD_REQ);
    }
    else
    {
      if(IS_OSPF6_SIBLING_DEBUG_CTRL_CLIENT)
      {
        zlog_debug("Entered a bad state, current_state: %d, new_state: %d", ctrl_client->state, new_state);
        return;
//        exit(1);
      }
    }
     
  }
  else
  {
    ctrl_client->state = new_state;  
  }
}

void ctrl_client_interface_init(struct ctrl_client * ctrl_client, char * interface_name)
{
  ctrl_client->interface_name = calloc(strlen(interface_name), sizeof(char));

  strncpy(ctrl_client->interface_name, interface_name, strlen(interface_name));
}

void ctrl_client_stop(struct ctrl_client * ctrl_client)
{
  /* Stop threads */
  THREAD_OFF(ctrl_client->t_read);
  THREAD_OFF(ctrl_client->t_write);
  THREAD_OFF(ctrl_client->t_connect);

  rfpbuf_delete(ctrl_client->ibuf);
  ctrl_client->ibuf = NULL;

  rfpbuf_delete(ctrl_client->obuf);
  ctrl_client->obuf = NULL;

  if(ctrl_client->sock >= 0)
  {
    close(ctrl_client->sock);
    ctrl_client->sock = -1;
  }

//  ctrl_client->fail = 0;
}

int ctrl_client_socket(struct in6_addr * ctrl_addr, struct in6_addr * sibling_addr)
{
  char c_addr[INET6_ADDRSTRLEN+1];
  int sock;
  int ret;
  int status;

  inet_ntop(AF_INET6, ctrl_addr, c_addr, INET6_ADDRSTRLEN+1);

  // Set up socket address info
  struct addrinfo hints, *addr;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET6;     // IPv6
  hints.ai_socktype = SOCK_STREAM;  // TCP

  // Set up source address
  struct sockaddr_in6 sin6;
  memset(&sin6, 0, sizeof(struct sockaddr_in6));
  sin6.sin6_family = AF_INET6;
  memcpy(&sin6.sin6_addr, sibling_addr, sizeof(struct in6_addr));
  sin6.sin6_port = 0; // specify an unused local port

  char port_str[8];
  sprintf(port_str, "%u", OSPF6_CTRL_SISIS_PORT);
  if((status = getaddrinfo(c_addr, port_str, &hints, &addr)) != 0)
  {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    exit(1);
  }

  sock = socket (addr->ai_family, addr->ai_socktype, addr->ai_protocol);
  if (sock < 0)
  {
    printf("Network: can't create socket.\n");
  }

  ret = bind(sock, (struct sockaddr *)&sin6, sizeof sin6);
  if(ret < 0)
  {
    perror("bind");
  }

  printf("ctrl_client TCP socket: creating socket %d\n", sock);

  /* Connect to controller. */
  ret = connect (sock, addr->ai_addr, addr->ai_addrlen);
  if (ret < 0)
  {
      close (sock);
      perror("connect");
      return -1;
  }

  return sock;
}

static int
ctrl_client_failed(struct ctrl_client * ctrl_client)
{
  ctrl_client->fail++;
  ctrl_client_stop(ctrl_client);
  ctrl_client_event(CTRL_CLIENT_CONNECT, ctrl_client);
  return -1;
}

int ctrl_send_message(struct ctrl_client * ctrl_client)
{
  if(ctrl_client->sock < 0)
    return -1;
  switch(rfpbuf_write(ctrl_client->obuf, ctrl_client->sock))
  {
    case RFPBUF_ERROR:
      break;

    case RFPBUF_EMPTY:
      break;
  }

  rfpbuf_delete(ctrl_client->obuf);
  ctrl_client->obuf = NULL;

  return 0;
}

/* Send simple message 
   where only header is 
   necessary to be filled */
static int ctrl_message_send(struct ctrl_client * ctrl_client, uint8_t type, uint8_t version)
{
  ctrl_client->obuf = routeflow_alloc(type, version, sizeof(struct rfp_header));

  return ctrl_send_message(ctrl_client);  
}

int fwd_message_send(struct ctrl_client * ctrl_client)
{
  return ctrl_send_message(ctrl_client);
}

/* connection to controller daemon */
int ctrl_client_start(struct ctrl_client * ctrl_client)
{
  int retval;

  if(ctrl_client->sock >= 0)
    return 0;

  if(ctrl_client->t_connect)
    return 0;

  /* Make socket */
  ctrl_client->sock = ctrl_client_socket (ctrl_client->ctrl_addr, ctrl_client->sibling_addr);
  if (ctrl_client->sock < 0)
  {
    printf("ctrl_client connection fail\n");
    ctrl_client->fail++;
    ctrl_client_event (CTRL_CLIENT_CONNECT, ctrl_client);
    return -1;
  }

  if(set_nonblocking(ctrl_client->sock) < 0)
  {
    printf("set_nonblocking failed\n");
  }

  /* Clear fail count */
  ctrl_client->fail = 0;

  ctrl_client_event(CTRL_CLIENT_READ, ctrl_client);
 
  // don't increment the xid on hellos
  ctrl_client->obuf = routeflow_alloc_xid(RFPT_HELLO, RFP10_VERSION, 
                                          htonl(ctrl_client->current_xid), sizeof(struct rfp_hello));

  retval = ctrl_send_message(ctrl_client);
  if(!retval)
    ctrl_client_state_transition(ctrl_client, CTRL_SENT_HELLO);
  
  return 0; 
}

/* wrapper function for threads */
static int ctrl_client_connect(struct thread * t)
{
  struct ctrl_client * ctrl_client;

  ctrl_client = THREAD_ARG(t);
  ctrl_client->t_connect = NULL;

  return ctrl_client_start(ctrl_client);
}

static int ctrl_client_read(struct thread * t)
{
  int ret;
//  timestamp_t timestamp;
  size_t already = 0;
  struct rfp_header * rh;
  struct ospf6_header * oh;
  unsigned int xid;
  uint16_t rfp6_length;
  uint16_t length;
  uint8_t type;
  size_t rh_size = sizeof(struct rfp_header);
  struct ctrl_client * ctrl_client = THREAD_ARG(t);
  struct interface * ifp;
  struct ospf6_interface * oi;

  ctrl_client->t_read = NULL;

  if(IS_OSPF6_SIBLING_DEBUG_CTRL_CLIENT)
  {
    zlog_debug("Ctrl client addr: %p", ctrl_client);
  }

  if(ctrl_client->ibuf == NULL)
  {
    ctrl_client->ibuf = rfpbuf_new(rh_size);
  }
  else
  {
    rfpbuf_put_uninit(ctrl_client->ibuf, rh_size);
  }

  ssize_t nbyte;
  if(((nbyte = rfpbuf_read_try(ctrl_client->ibuf, ctrl_client->sock, sizeof(struct rfp_header)-already)) == 0) ||
     (nbyte == -1))
  {
    return ctrl_client_failed(ctrl_client);
  }
  if(nbyte != (ssize_t)(sizeof(struct rfp_header)-already))
  {
    /* Try again later */
    ctrl_client_event(CTRL_CLIENT_READ, ctrl_client);
    return 0;
  }

  already = rh_size;
 
  rh = rfpbuf_at_assert(ctrl_client->ibuf, 0, sizeof(struct rfp_header));
  length = ntohs(rh->length);
  type = rh->type;
  rfpbuf_prealloc_tailroom(ctrl_client->ibuf, length - already);  // make sure we have enough space for the body

  if(already < length)
  {
    if(((nbyte = rfpbuf_read_try(ctrl_client->ibuf, ctrl_client->sock, length - already)) == 0) ||
       (nbyte == -1))
    {
      return ctrl_client_failed(ctrl_client);
    }

    if(nbyte != (ssize_t) length - already)
    {
      /* Try again later */
      ctrl_client_event(CTRL_CLIENT_READ, ctrl_client);
      return 0;
    }
  }

  length -= rh_size;

  switch(type)
  {
    case RFPT_HELLO:
      if(IS_OSPF6_SIBLING_DEBUG_CTRL_CLIENT)
      {
        zlog_debug("Hello message received");
      }

      if(ctrl_client->state == CTRL_SENT_HELLO)
        ctrl_client_state_transition(ctrl_client, CTRL_RCVD_HELLO);
  
      ctrl_client_event(CTRL_CLIENT_CONNECTED, ctrl_client);
      break;

    case RFPT_LEADER_ELECT:
      if(IS_OSPF6_SIBLING_DEBUG_CTRL_CLIENT)
      {
        zlog_debug("Leader elect message received");
      }
      ctrl_client_state_transition(ctrl_client, CTRL_LEAD_ELECT_RCVD);
      break;
    
    case RFPT_FEATURES_REPLY:
      ret = ctrl_client->features_reply(ctrl_client, ctrl_client->ibuf);

      ctrl_client_if_addr_req(ctrl_client);
      break;

    case RFPT_IPV4_ADDRESS_ADD:
      ret = ctrl_client->address_add_v4(ctrl_client, ctrl_client->ibuf);
      break;

    case RFPT_IPV6_ADDRESS_ADD:
      ret = ctrl_client->address_add_v6(ctrl_client, ctrl_client->ibuf);
      break;

    case RFPT_GET_CONFIG_REPLY:
      break;

    case RFPT_IPV4_ROUTE_ADD:
      ret = ctrl_client->routes_reply(ctrl_client, ctrl_client->ibuf);
      break;

    case RFPT_FORWARD_OSPF6:
      if(IS_OSPF6_SIBLING_DEBUG_CTRL_CLIENT)
      {
        zlog_debug("Forwarding message received");
      }

      pthread_mutex_lock(&restart_mode_mutex);
      if(!ospf6->restart_mode)
      {
        pthread_mutex_unlock(&restart_mode_mutex);
        rh = rfpbuf_at_assert(ctrl_client->ibuf, 0, sizeof(struct rfp_header));
        ifp = if_get_by_name(ctrl_client->if_list, ctrl_client->interface_name);
        oi = (struct ospf6_interface *)ifp->info;
        oh = (struct ospf6_header *)((void *)rh + sizeof(struct rfp_header));
        xid = ntohl(rh->xid);
        ospf6_receive(ctrl_client, oh, xid, oi);
      }
      else
      {
        pthread_mutex_unlock(&restart_mode_mutex);
        rh = rfpbuf_at_assert(ctrl_client->ibuf, 0, sizeof(struct rfp_header));
        if(IS_OSPF6_SIBLING_DEBUG_CTRL_CLIENT)
        {
          zlog_debug("Process is in restart mode...");
        }

        struct rfpbuf * msg_rcvd = rfpbuf_clone(ctrl_client->ibuf);
        list_init(&msg_rcvd->list_node);
//        timestamp = sibling_ctrl_ingress_timestamp();
//        msg_rcvd->timestamp = timestamp;

        if(IS_OSPF6_SIBLING_DEBUG_CTRL_CLIENT)
        {
          zlog_debug("Start appending to msg_rcvd");
        }


        pthread_mutex_lock(&restart_msg_q_mutex);
        sibling_ctrl_push_to_restart_msg_queue(msg_rcvd);
        pthread_mutex_unlock(&restart_msg_q_mutex);

        if(IS_OSPF6_SIBLING_DEBUG_CTRL_CLIENT)
        {
          zlog_debug("Stop appending to msg_rcvd");
        }


        // unlock the restart thread so it knows what is the first message that has been received
        pthread_mutex_unlock(&first_xid_mutex);
      }
      break;
  }

  if(IS_OSPF6_SIBLING_DEBUG_CTRL_CLIENT)
  {
    zlog_debug("Before rfpbuf_delete");
  }

  rfpbuf_delete(ctrl_client->ibuf);
  ctrl_client->ibuf = NULL;

  if(IS_OSPF6_SIBLING_DEBUG_CTRL_CLIENT)
  {
    zlog_debug("End of ctrl_client");
  }

  ctrl_client_event(CTRL_CLIENT_READ, ctrl_client);
  return 0;
}

static int ctrl_client_connected(struct thread * t)
{
  int retval;
  struct ctrl_client * ctrl_client = THREAD_ARG(t);

  ctrl_client->obuf = routeflow_alloc_xid(RFPT_FEATURES_REQUEST, RFP10_VERSION, 
                                          htonl(ctrl_client->current_xid), sizeof(struct rfp_header));
  retval = ctrl_send_message(ctrl_client); 
  ctrl_client->current_xid++;

  ctrl_client->obuf = routeflow_alloc_xid(RFPT_REDISTRIBUTE_REQUEST, RFP10_VERSION, 
                                          htonl(ctrl_client->current_xid), sizeof(struct rfp_header));
  retval = ctrl_send_message(ctrl_client);
  ctrl_client->current_xid++;

  return 0;
}

int ctrl_client_route_set(struct ctrl_client * ctrl_client, struct ospf6_route * route)
{
  int retval;
  char prefix_str[INET6_ADDRSTRLEN];
  char nexthop_str[INET6_ADDRSTRLEN];


  if(IS_OSPF6_SIBLING_DEBUG_CTRL_CLIENT)
  {
    inet_ntop(AF_INET6, &(route->prefix.u.prefix6.s6_addr), prefix_str, INET6_ADDRSTRLEN);
    zlog_debug("Route Set: %s for %i", prefix_str, ctrl_client->hostnum);
    
    inet_ntop(AF_INET6, &(route->nexthop[0].address), nexthop_str, INET6_ADDRSTRLEN);
    zlog_debug("          Nexthop addr: %s", nexthop_str);

    zlog_debug("          ifindex: %i", route->nexthop[0].ifindex);
  }

  /* Only the best path will be sent to zebra. */
  if(!ospf6_route_is_best(route))
  {   
    /* this is not preferred best route, ignore */
    if (IS_OSPF6_SIBLING_DEBUG_CTRL_CLIENT)
      zlog_debug ("  Ignore non-best route");
    return;
  }
                            
  ctrl_client->obuf = routeflow_alloc_xid(RFPT_IPV6_ROUTE_SET_REQUEST, RFP10_VERSION,
                                          htonl(ctrl_client->current_xid), sizeof(struct rfp_ipv6_route));

  struct rfp_ipv6_route * rir = rfpbuf_at_assert(ctrl_client->obuf, 0, sizeof(struct rfp_ipv6_route));
  rir->prefixlen = htons(route->prefix.prefixlen);
  memcpy(&rir->p, &route->prefix.u.prefix, sizeof(struct in6_addr));
  rir->ifindex = htons(route->nexthop[0].ifindex);
  memcpy(&rir->nexthop_addr, &route->nexthop[0].address, sizeof(struct in6_addr));

  retval = ctrl_send_message(ctrl_client);
  ctrl_client->current_xid++;

  return 0;
}

int ctrl_client_route_unset(struct ctrl_client * ctrl_client, struct ospf6_route * route)
{
  int retval;
  char prefix_str[INET6_ADDRSTRLEN];
  char nexthop_str[INET6_ADDRSTRLEN];

  if(IS_OSPF6_SIBLING_DEBUG_CTRL_CLIENT)
  {
    inet_ntop(AF_INET6, &(route->prefix.u.prefix6.s6_addr), prefix_str, INET6_ADDRSTRLEN);
    zlog_debug("Route UnSet: %s for %i", prefix_str, ctrl_client->hostnum);

    inet_ntop(AF_INET6, &(route->nexthop[0].address), nexthop_str, INET6_ADDRSTRLEN);
    zlog_debug("          Nexthop addr: %s", nexthop_str);

    zlog_debug("          ifindex: %i", route->nexthop[0].ifindex);
   }

  /* if removing is the best path and if there's another path,
     treat this request as add the secondary path */
  if(ospf6_route_is_best(route) && route->next && ospf6_route_is_same(route, route->next))
  {
    if(IS_OSPF6_SIBLING_DEBUG_CTRL_CLIENT)
    {
      zlog_debug("  Best-path removal resulted in Secondary addition");
    }
  }

  return 0;
}

int ctrl_client_if_addr_req(struct ctrl_client * ctrl_client)
{
  int retval;

  ctrl_client->obuf = routeflow_alloc_xid(RFPT_IF_ADDRESS_REQ, RFP10_VERSION,
                                          htonl(ctrl_client->current_xid), sizeof(struct rfp_header));
  retval = ctrl_send_message(ctrl_client);
  ctrl_client->current_xid++;

  return 0;
}

static void 
ctrl_client_event(enum event event, struct ctrl_client * ctrl_client)
{
  switch(event)
  {
    case CTRL_CLIENT_SCHEDULE:
      ctrl_client->t_connect = thread_add_event(master, ctrl_client_connect, ctrl_client, 0);
      break;
    case CTRL_CLIENT_CONNECT:
      // TODO: implement connecting again
      break;
    case CTRL_CLIENT_READ:
      ctrl_client->t_read = thread_add_read(master, ctrl_client_read, ctrl_client, ctrl_client->sock);
      break;
    case CTRL_CLIENT_CONNECTED:
      ctrl_client->t_connected = thread_add_event(master, ctrl_client_connected, ctrl_client, 0);
      break;
  }
}

void ctrl_client_redistribute_leader_elect(struct ctrl_client * ctrl_client, bool leader)
{
  int retval;
  struct red_lead_elec * red_lead_elec;
  uint8_t * is_leader_p;

  // don't increment the xid on redistribution of leader elect msgs
  ctrl_client->obuf = routeflow_alloc_xid(RFPT_REDISTRIBUTE_LEADER_ELECT, RFP10_VERSION, 
                                          htonl(ctrl_client->current_xid), sizeof(struct rfp_header));

  red_lead_elec = rfpbuf_put_uninit(ctrl_client->obuf, sizeof(struct red_lead_elec));;

  red_lead_elec->is_leader = leader == true ? 1 : 0;

  rfpmsg_update_length(ctrl_client->obuf);
  retval = ctrl_send_message(ctrl_client);

  rfpbuf_delete(ctrl_client->obuf);
  ctrl_client->obuf = NULL;
}
