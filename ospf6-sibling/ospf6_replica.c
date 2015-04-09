#include "config.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>

#include "util.h"
#include "dblist.h"
#include "debug.h"
#include "vector.h"
#include "vty.h"
#include "command.h"
#include "prefix.h"
#include "sisis_api.h"
#include "sisis.h"
#include "sisis_process_types.h"
#include "routeflow-common.h"
#include "rfpbuf.h"
#include "rfp-msgs.h"
#include "thread.h"
#include "ospf6_replica.h"
#include "ospf6_interface.h"
#include "ospf6_route.h"
#include "sibling_ctrl.h"

enum event {REPLICA_READ};

extern struct thread_master * master;

void ospf6_replica_restart(unsigned int id);
static bool ospf6_replica_own(struct in6_addr * addr);
static struct sibling * find_replica_from_addr(struct in6_addr * addr);
static void send_ids();
static int ospf6_replica_send_msg(unsigned int);
static void ospf6_replica_event(enum event);

static struct ospf6_replica * ospf6_replica;

void ospf6_replicas_restart();

DEFUN(id,
      id_cmd,
      "id IDNUM",
      DEBUG_STR
      "OSPF6 Sibling configuration\n"
      "Debug MSG events\n")
{
  ospf6_replica->own_replica->id = (unsigned int) strtol(argv[0], NULL, 10);

  return CMD_SUCCESS; 
}

int rib_monitor_add_ipv4_route(struct route_ipv4 * route, void * data)
{
  if(IS_OSPF6_SIBLING_DEBUG_REPLICA)
  {
    char prefix_str[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &(route->p->prefix.s_addr), prefix_str, INET_ADDRSTRLEN) != 1)
      zlog_debug("Added route: %s/%d [%u/%u]", prefix_str, route->p->prefixlen, route->distance, route->metric);
  }                 

  // Free memory
  free(route);
}

int rib_monitor_remove_ipv4_route(struct route_ipv4 * route, void * data)
{
  if(IS_OSPF6_SIBLING_DEBUG_SISIS)
  {
    char prefix_str[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &(route->p->prefix.s_addr), prefix_str, INET_ADDRSTRLEN) != 1)
      zlog_debug("Removed route: %s/%d [%u/%u]", prefix_str, route->p->prefixlen, route->distance, route->metric);
  }

  // Free memory
  free(route);
}

int rib_monitor_add_ipv6_route(struct route_ipv6 * route, void * data)
{
  if(IS_OSPF6_SIBLING_DEBUG_REPLICA)
  {
    char prefix_str[INET6_ADDRSTRLEN];
    if (inet_ntop(AF_INET6, &(route->p->prefix.s6_addr), prefix_str, INET6_ADDRSTRLEN) != 1)
      zlog_debug("Added route: %s/%d [%u/%u]", prefix_str, route->p->prefixlen, route->distance, route->metric);
  }

  // Free memory
  free(route);
}

int rib_monitor_remove_ipv6_route(struct route_ipv6 * route, void * data)
{
  struct sibling * sibling;

  if(IS_OSPF6_SIBLING_DEBUG_REPLICA)
  {
    char prefix_str[INET6_ADDRSTRLEN];
    if (inet_ntop(AF_INET6, &(route->p->prefix), prefix_str, INET6_ADDRSTRLEN) != 1)
      zlog_debug("Removed route: %s/%d [%u/%u]", prefix_str, route->p->prefixlen, route->distance, route->metric);
  }
 
  sibling = find_replica_from_addr(&route->p->prefix);

  // Free memory
  free(route);

  if(sibling)
  {
    sibling->valid = false; // may need to wrap this around in a mutex

    if(IS_OSPF6_SIBLING_DEBUG_REPLICA)
    {
      zlog_debug("The sibling corresponding to removed route has id: %d", sibling->id);
    }

    if(ospf6_replica->own_replica->leader)
    {
      // try to restart the replica
      ospf6_replica_restart(sibling->id);
    }
  }
}

static struct sibling * ospf6_create_replica(struct in6_addr * addr, bool own)
{
  struct sibling * sibling = calloc(1, sizeof(struct sibling));
  sibling->sibling_addr = calloc(1, sizeof(struct in6_addr));
  memcpy(sibling->sibling_addr, addr, sizeof(struct in6_addr));
  sibling->leader = false;
  sibling->valid = true;
 
  if(own)
  {
    // copy the address to the grobal struct
    ospf6_replica->own_replica = sibling;

    if((ospf6_replica->sock = ospf6_replica_socket(addr)) < 0)
    {
      // error
    }

    if(set_nonblocking(ospf6_replica->sock) < 0)
    {
      printf("set_nonblocking failed\n");
    }

    ospf6_replica_event(REPLICA_READ); 
  }
  else
  {
    struct sibling * own_replica = ospf6_replica->own_replica;

    // create a socket that corresponds to the sibling
    sibling->sock = ospf6_sibling_socket(own_replica->sibling_addr, sibling->sibling_addr);

    // push back the new address to the list
    list_init(&sibling->node);
    list_push_back(&ospf6_replica->replicas, &sibling->node);
  }

  return sibling;
}

static void ospf6_add_replicas(struct list * replicas)
{
  struct route_ipv6 * route_iter;
  LIST_FOR_EACH(route_iter, struct route_ipv6, node, replicas)
  {
    if(!ospf6_replica_own(&route_iter->p->prefix))
    {
      ospf6_create_replica(&route_iter->p->prefix, false);
    }
  }
}

static struct sibling * ospf6_update_replicas(struct in6_addr * addr)
{
  struct sibling * sibling;
  if((sibling = find_replica_from_addr(addr)) == NULL)
  {
    sibling = ospf6_create_replica(addr, false);
  }

  return sibling;
}

static bool ospf6_replica_own(struct in6_addr * addr)
{
  if(memcmp(addr, ospf6_replica->own_replica->sibling_addr, sizeof(struct in6_addr)) == 0)
  {
    return true;
  }

  return false;
}

static void ospf6_remove_replica(struct sibling * sibling)
{
  free(sibling->sibling_addr);

  list_remove(&sibling->node);

  free(sibling);
}

static struct sibling * find_replica_from_addr(struct in6_addr * addr)
{
  struct sibling * sibling_iter;
  LIST_FOR_EACH(sibling_iter, struct sibling, node, &ospf6_replica->replicas)
  {
    if(sibling_iter->valid == false)
    {
      ospf6_remove_replica(sibling_iter);
    }
    else
    {
      if(memcmp(sibling_iter->sibling_addr, addr, sizeof(struct in6_addr)) == 0)
      {
        return sibling_iter;
      }
    }
  }

  return NULL;
}

int ospf6_leader_elect()
{
  char sisis_addr[INET6_ADDRSTRLEN];
  unsigned int num_of_siblings = number_of_sisis_addrs_for_process_type(SISIS_PTYPE_OSPF6_SBLING);
  struct sibling * own_replica = ospf6_replica->own_replica;
   

  if(num_of_siblings == OSPF6_NUM_SIBS)
  {
    inet_ntop(AF_INET6, own_replica->sibling_addr, sisis_addr, INET6_ADDRSTRLEN);

    uint64_t own_prefix, own_sisis_version, own_process_type, own_process_version, own_sys_id, own_pid, own_ts; 
    if(get_sisis_addr_components(sisis_addr, 
                                 &own_prefix, 
                                 &own_sisis_version, 
                                 &own_process_type, 
                                 &own_process_version, 
                                 &own_sys_id, 
                                 &own_pid, 
                                 &own_ts) == 0)
    {
      bool oldest_sibling = true;
      struct list * sibling_addrs = get_ospf6_sibling_addrs();
      struct route_ipv6 * route_iter;
      LIST_FOR_EACH(route_iter, struct route_ipv6, node, sibling_addrs)
      {
        char addr[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &route_iter->p->prefix, addr, INET6_ADDRSTRLEN);
        struct sibling * sibling;

        if(memcmp(&route_iter->p->prefix, own_replica->sibling_addr, sizeof(struct in6_addr)) == 0)
        {
          // we came across our own timestamp
          continue;
        }
        else
        {
          uint64_t prefix, 
                   sisis_version, 
                   process_type, 
                   process_version, 
                   sys_id, 
                   pid, 
                   ts; 
          if(get_sisis_addr_components(addr, &prefix, &sisis_version, &process_type, &process_version, &sys_id, &pid, &ts) == 0)
          {
            if(ts < own_ts)
            {
              // this sibling does not have the smallest timestamp
              oldest_sibling = false;
            }
          }
          else 
          {
            perror("Leader election");
          }

          if(IS_OSPF6_SIBLING_DEBUG_REPLICA)
          {
            zlog_debug("pushing sibling for replica %s", addr);
          }

          // create a sibling if already not created, not our own
          sibling = ospf6_update_replicas(&route_iter->p->prefix);

          if(ts == own_ts && sibling->leader)
          {
            // assume that if leader for a sibling is filled in,
            // then it was received from the id message
            // we have the same timestamp, use ids for tie breaking
            if(sibling->id < own_replica->id)
            {
              oldest_sibling = false;
            }
          }
        }
      }

      if(oldest_sibling)
      {
        if(IS_OSPF6_SIBLING_DEBUG_REPLICA)
        {
          zlog_debug("I am the oldest sibling");
          own_replica->leader = true;

        }
      }
      else
      {
        if(IS_OSPF6_SIBLING_DEBUG_REPLICA)
        {
          zlog_debug("I am not the oldest sibling");
          own_replica->leader = false;
        }
      }

      send_ids();
    }
    else
    {
      // couldn't get components of own sisis addr
    }
  }
  else
  {
    printf("Not enough siblings\n");
    exit(1);
  }
}

/* Create listening socket for ourselves */
int ospf6_replica_socket(struct in6_addr * input_addr)
{
  int sockfd;
  char r_addr[INET6_ADDRSTRLEN];
  char port_str[8];
  int status;
  int ret;
  struct addrinfo hints, *addr;

  inet_ntop(AF_INET6, input_addr, r_addr, INET6_ADDRSTRLEN);

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET6;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = IPPROTO_UDP;

  sprintf(port_str, "%u", OSPF6_SIBLING_PORT);
  if((status = getaddrinfo(r_addr, port_str, &hints, &addr)) != 0)
  {
    // error
    exit(1);
  }

  sockfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
  if(sockfd < 0)
  {
    perror("socket");
  }

  ret = bind(sockfd, addr->ai_addr, addr->ai_addrlen);
  if(ret < 0)
  {
    perror("bind");
  }

//  if((ret = connect(sockfd, addr->ai_addr, addr->ai_addrlen)) < 0)
//  {
//    perror("connect");
//  }

  return sockfd;
}

/* Create a socket to the sibling we would like to connect 
   to through UDP */
int ospf6_sibling_socket(struct in6_addr * own_addr, struct in6_addr * sibling_addr)
{
  int sockfd;
  char r_addr[INET6_ADDRSTRLEN];
  char port_str[8];
  int status;
  int ret;
  struct addrinfo hints, *addr;

  inet_ntop(AF_INET6, sibling_addr, r_addr, INET6_ADDRSTRLEN);

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET6;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = IPPROTO_UDP;

  // Set up source address
  struct sockaddr_in6 sin6;
  memset(&sin6, 0, sizeof(struct sockaddr_in6));
  sin6.sin6_family = AF_INET6;
  memcpy(&sin6.sin6_addr, own_addr, sizeof(struct in6_addr));
  sin6.sin6_port = 0; // specify an unused local port


  sprintf(port_str, "%u", OSPF6_SIBLING_PORT);
  if((status = getaddrinfo(r_addr, port_str, &hints, &addr)) != 0)
  {
    // error
    exit(1);
  }

  sockfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
  if(sockfd < 0)
  {
    perror("socket");
  }

  ret = bind(sockfd, (struct sockaddr *)&sin6, sizeof sin6);
  if(ret < 0)
  {
    perror("bind");
  }

  if((ret = connect(sockfd, addr->ai_addr, addr->ai_addrlen)) < 0)
  {
    perror("connect");
  }

  return sockfd;
}

static int ospf6_replica_failed()
{
  ospf6_replica->fail++;
  return -1;
}

static int ospf6_replica_send_msg(unsigned int sock)
{
  size_t numbytes;

  if(sock < 0)
    return -1;
  numbytes = send(sock, rfpbuf_at_assert(ospf6_replica->obuf, 0, ospf6_replica->obuf->size), ospf6_replica->obuf->size, 0);

  if(numbytes < 0)
  { 
    printf("send error");
  }

  return numbytes;
}

static void send_ids()
{
  int retval;
  struct ospf6_replica_ex * r_ex;
  struct sibling * own_replica = ospf6_replica->own_replica;

  ospf6_replica->obuf = routeflow_alloc_xid(RFPT_REPLICA_EX, RFP10_VERSION, htonl(0), sizeof(struct rfp_header));
  r_ex = rfpbuf_put_uninit(ospf6_replica->obuf, sizeof(struct ospf6_replica_ex));
  r_ex->id = htons(own_replica->id);
  r_ex->leader = own_replica->leader == true ? 1 : 0;
      
  rfpmsg_update_length(ospf6_replica->obuf);

  struct sibling * sibling_iter;
  LIST_FOR_EACH(sibling_iter, struct sibling, node, &ospf6_replica->replicas)
  {
    if(sibling_iter->valid)
    {
      char addr[INET6_ADDRSTRLEN];

      inet_ntop(AF_INET6, sibling_iter->sibling_addr, addr, INET6_ADDRSTRLEN);

      if(IS_OSPF6_SIBLING_DEBUG_REPLICA)
      {
        zlog_debug("Sending id %d, leader %d out to %s", own_replica->id, own_replica->leader == true ? 1 : 0, addr);
      }

    retval = ospf6_replica_send_msg(sibling_iter->sock);

    }
  }
    
  // Clean up after sending the messages
  rfpbuf_delete(ospf6_replica->obuf);
  ospf6_replica->obuf = NULL;

}

static void fill_id(struct sockaddr * addr, unsigned int leader, unsigned int id)
{
  struct sockaddr_in6 * ip6addr;
  struct sibling * sibling;

  if(addr->sa_family == AF_INET6)
  {
    ip6addr = (struct sockaddr_in6 *)addr;
    if((sibling = find_replica_from_addr(&ip6addr->sin6_addr)) != NULL)
    {
      // for debugging
      char s_addr[INET6_ADDRSTRLEN];
      inet_ntop(AF_INET6, sibling->sibling_addr, s_addr, INET6_ADDRSTRLEN);

      sibling->id = id;

      sibling->leader = leader == 1 ? true : false;

      if(IS_OSPF6_SIBLING_DEBUG_REPLICA)
      {
        zlog_debug("sibling %s now has id set to %d, leader: %d", s_addr, sibling->id, leader);
      }

      if(leader && ospf6_replica->own_replica->leader)
      {
        // we have more than one leader, let the id be our tiebreaker
        if(ospf6_replica->own_replica->id < sibling->id)
        {
          sibling->leader = false;

          if(IS_OSPF6_SIBLING_DEBUG_REPLICA)
          {
            zlog_debug("changed %s to not be leader", s_addr);
          }
        }
        else
        {
          ospf6_replica->own_replica->leader = false;
          if(IS_OSPF6_SIBLING_DEBUG_REPLICA)
          {
            zlog_debug("changed own replica to not be leader");
          }
        }
      }
    }
    else
    {
      if(IS_OSPF6_SIBLING_DEBUG_REPLICA)
      {
        char s_addr[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &ip6addr->sin6_addr, s_addr, INET6_ADDRSTRLEN);

        zlog_debug("could not find sibling to set id to: %s", s_addr);
        sibling = ospf6_create_replica(&ip6addr->sin6_addr, false);
        sibling->leader = leader;
        sibling->id = id;
      }
    }
  }
  else
  {
    if(IS_OSPF6_SIBLING_DEBUG_REPLICA)
    {
      zlog_debug("udp received on an address which is not ipv6");
    }
  }

  // bug 
  if(IS_OSPF6_SIBLING_DEBUG_REPLICA)
  {
    zlog_debug("About to update sibling => Leader Election Complete");
  }
  sibling_ctrl_update_state(SCG_LEAD_ELECT_COMPL);
}

static int ospf6_replica_read(struct thread * t)
{
  int ret;
  struct rfp_header * rh;
  struct ospf6_replica_ex * r_ex;
  uint16_t rfp6_length;
  uint16_t length;
  uint8_t type;
  unsigned int id;
  unsigned int leader;
  size_t rh_size = sizeof(struct rfp_header);

  if(IS_OSPF6_SIBLING_DEBUG_REPLICA)
  {
    zlog_debug("ospf6_replica_read called");
  }

  ospf6_replica->t_read = NULL;

  if(ospf6_replica->ibuf == NULL)
  {
    ospf6_replica->ibuf = rfpbuf_new(rh_size);
  }
  else
  {
    rfpbuf_put_uninit(ospf6_replica->ibuf, rh_size);
  }

  ssize_t nbyte;
  struct sockaddr_storage src;
  socklen_t len = sizeof(src);

  void * p = rfpbuf_tail(ospf6_replica->ibuf);

  // Peek into the packet just to read the header
  if(((nbyte = recvfrom(ospf6_replica->sock, p, rh_size, MSG_PEEK, (struct sockaddr *)&src, &len)) == 0) ||
      (nbyte == -1))
  {
    return ospf6_replica_failed();
  }

  ospf6_replica->ibuf->size += nbyte;

  if(nbyte != (ssize_t)(rh_size))
  {
    /* Try again later */
    ospf6_replica_event(REPLICA_READ);
    return 0;
  }

  rh = rfpbuf_at_assert(ospf6_replica->ibuf, 0, sizeof(struct rfp_header));
  length = ntohs(rh->length);

  // reinitialize the data (so we can receive the header as well as data)
  rfpbuf_delete(ospf6_replica->ibuf);
  ospf6_replica->ibuf = rfpbuf_new(length);

  if(IS_OSPF6_SIBLING_DEBUG_REPLICA)
  {
    zlog_debug("received replica msg (type: %d, length: %d)", type, length);
  }

  p = rfpbuf_tail(ospf6_replica->ibuf);
  
  // receive the full packet
  if(((nbyte = recvfrom(ospf6_replica->sock, p, length, 0, (struct sockaddr *)&src, &len)) == 0) ||
     (nbyte == -1))
  {
    printf("errno is set to %d\n", errno);

    if(errno == 11)
    {
      // EAGAIN - Try again later */
      ospf6_replica_event(REPLICA_READ);
      return 0;
    }

    return ospf6_replica_failed();
  }

  ospf6_replica->ibuf->size += nbyte;

  if(nbyte != (ssize_t) length)
  {
    /* Try again later */
    ospf6_replica_event(REPLICA_READ);
    return 0;
  }

  length -= rh_size;

  // reinitialize the header
  rh = rfpbuf_at_assert(ospf6_replica->ibuf, 0, rh_size);
  type = rh->type;
  
  switch(type)
  {
    case RFPT_REPLICA_EX:
      r_ex = rfpbuf_at_assert(ospf6_replica->ibuf, rh_size, sizeof(struct ospf6_replica_ex));

      if(IS_OSPF6_SIBLING_DEBUG_REPLICA)
      {
        zlog_debug("received RFPT_REPLICA_EX: id %d, leader %d", ntohs(r_ex->id), r_ex->leader);
      }

      id = ntohs(r_ex->id);
      leader = r_ex->leader;
      fill_id((struct sockaddr *)&src, leader, id);


      break;
  }

  rfpbuf_delete(ospf6_replica->ibuf);
  ospf6_replica->ibuf = NULL;

  ospf6_replica_event(REPLICA_READ);
}

void ospf6_replica_restart(unsigned int id)
{
  pid_t pid;
  int ret;
  char num_id[15];

  sprintf(num_id, "%d", id);
  char * args[] = {"qsub", "-V", "-t", num_id, "-d", "/home/hasenov/zebralite/ospf6-sibling", "restart_sibling.sh", NULL};
  
//  char *args[] = {"echo", "Hello world", NULL};

  if(IS_OSPF6_SIBLING_DEBUG_REPLICA)
  {
    zlog_debug("About to restart sibling with id %d", id);
  }
  // temporarily disable

  if((pid = vfork()) == 0)
  {
    if((ret = execvp("qsub", args)) < 0)
    {
      zlog_debug("error on execvp");
    }
      
  }

  if(IS_OSPF6_SIBLING_DEBUG_REPLICA)
  {
    zlog_debug("Return from fork");
  }
}

unsigned int ospf6_replica_get_id()
{
  return ospf6_replica->own_replica->id;
}

unsigned int ospf6_replica_get_leader_id()
{
  struct sibling * sibl;
  LIST_FOR_EACH(sibl, struct sibling, node, &ospf6_replica->replicas)
  {
    if(sibl->leader)
    {
      return sibl->id;
    }
  }

  printf("Error!\n");
  exit(1);
}

void ospf6_replica_check_for_slowness()
{
  unsigned int my_id;
  int my_xid;
  int xid_of_other;
  int id_diff;

  int slower = 0; // this replica is slower from how many other replicas
  int threshold = 5;  // for testing, make the threshold be 5
  
  my_id = ospf6_replica_get_id();
  my_xid = ospf6_db_get_current_xid(my_id);

  struct sibling * sibl;
  LIST_FOR_EACH(sibl, struct sibling, node, &ospf6_replica->replicas)
  {
    if(sibl->id != my_id)
    {
      // current sibling is different
      xid_of_other = ospf6_db_get_current_xid(sibl->id);
      id_diff = xid_of_other - my_xid;
      if(id_diff < 0)
      {
        continue; 
      }

      if(id_diff > threshold)
      {
        slower++;
      }
    }
  }
  if(((float)slower/OSPF6_NUM_SIBS) > 0.5)
  {
    zlog_debug("I am too slow, will need to catch up to others");
  }
}

void ospf6_replicas_init(struct in6_addr * own_replica_addr, struct list * replicas)
{
  ospf6_replica = calloc(1, sizeof(struct ospf6_replica));
  list_init(&ospf6_replica->replicas);

  ospf6_create_replica(own_replica_addr, true);

  ospf6_add_replicas(replicas);

  install_element(CONFIG_NODE, &id_cmd);
}

static void ospf6_replica_event(enum event event)
{
  switch(event)
  {
    case REPLICA_READ:
      ospf6_replica->t_read = thread_add_read(master, ospf6_replica_read, NULL, ospf6_replica->sock);
      break;
  }
}
