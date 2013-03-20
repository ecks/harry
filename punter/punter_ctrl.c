#include "config.h"

#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "stdbool.h"
#include "stddef.h"
#include "arpa/inet.h"
#include "linux/if.h"
#include "netinet/in.h"

#include "routeflow-common.h"
#include "util.h"
#include "dblist.h"
#include "prefix.h"
#include "api.h"
#include "ext_client.h"
#include "zl_client.h"
#include "punter_ctrl.h"

static struct ext_client * ext_client = NULL;
static struct zl_client * zl_client = NULL;

static struct punter_ctrl * punter_ctrl = NULL;

int add_ipv4_addr(int index, void * address, u_char prefixlen, struct list * list)
{
  struct addr_ipv4 * addr = calloc(1, sizeof(struct addr_ipv4));
  addr->ifindex = index;
  printf("ifindex %d\n", addr->ifindex); 

  addr->p = calloc(1, sizeof(struct prefix_ipv4));
  addr->p->family = AF_INET;
  memcpy(&addr->p->prefix, address, 4);
  addr->p->prefixlen = prefixlen;

  // print route
  char prefix_str[INET_ADDRSTRLEN];
  if (inet_ntop(AF_INET, &(addr->p->prefix.s_addr), prefix_str, INET_ADDRSTRLEN) != NULL)
      printf("%s/%d\n", prefix_str, addr->p->prefixlen);

  list_push_back(list, &addr->node);

  return 0;
}

int remove_ipv4_addr(int index, struct list * list)
{
  return 0;
}

#ifdef HAVE_IPV6
int add_ipv6_addr(int index, void * address, u_char prefixlen, struct list * list)
{
  struct addr_ipv6 * addr = calloc(1, sizeof(struct addr_ipv6));
  addr->ifindex = index;

  printf("ifindex %d\n", addr->ifindex); 

  addr->p = calloc(1, sizeof(struct prefix_ipv6));
  addr->p->family = AF_INET6;
  memcpy(&addr->p->prefix, address, 16);
  addr->p->prefixlen = prefixlen;
  
  // print route
  char prefix_str[INET6_ADDRSTRLEN];
  if (inet_ntop(AF_INET6, &(addr->p->prefix.s6_addr), prefix_str, INET6_ADDRSTRLEN) != NULL)
      printf("%s/%d\n", prefix_str, addr->p->prefixlen);
  
  list_push_back(list, &addr->node);

  return 0;
}

int remove_ipv6_addr(int index, struct list * list)
{
  return 0;
}
#endif

void get_addrs(struct list * ipv4_addrs, struct list * ipv6_addrs)
{
  if(addrs_list(punter_ctrl->ipv4_addrs, punter_ctrl->ipv6_addrs, add_ipv4_addr, remove_ipv4_addr, add_ipv6_addr, remove_ipv6_addr) < 0)
  {
    exit(1);
  }
}

int iface_add_port (int index, unsigned int flags, unsigned int mtu, char * name, struct list * list)
{
  struct sw_port * port = calloc(1, sizeof(struct sw_port));
  if(port != NULL)
  {
    port->port_no =  index;
    port->mtu = mtu;
    if(flags & IFF_LOWER_UP && !(flags & IFF_DORMANT))
    {
      port->state = RFPPS_FORWARD;
    }
    else
    {
      port->state = RFPPS_LINK_DOWN;
    }
    strncpy(port->hw_name, name, strlen(name));
    list_push_back(list, &port->node);
  } 

  return 0;
}

void get_ports(struct list * ports)
{
 if(interface_list(ports, iface_add_port) != 0)
  {
    exit(1);
  }

  struct sw_port * port;
  LIST_FOR_EACH(port, struct sw_port, node, ports)
  {
    printf("Interface %u: %u %s\n", port->port_no, port->mtu, port->hw_name);
    if(port->state == RFPPS_FORWARD)
    {
      printf("Link is up!\n");
    }
    else
    {
      printf("Link is down!\n");
    }
  }
 
}

void punter_ctrl_init(char * host)
{
  punter_ctrl = calloc(1 , sizeof(struct punter_ctrl));
  punter_ctrl->ipv4_addrs = calloc(1, sizeof(struct list));
  list_init(punter_ctrl->ipv4_addrs);
 
#ifdef HAVE_IPV6
  punter_ctrl->ipv6_addrs = calloc(1, sizeof(struct list));
  list_init(punter_ctrl->ipv6_addrs);
#endif

  get_addrs(punter_ctrl->ipv4_addrs, punter_ctrl->ipv6_addrs);

  punter_ctrl->port_list = calloc(1, sizeof(struct list));
  list_init(punter_ctrl->port_list);

  get_ports(punter_ctrl->port_list);

  ext_client = ext_client_new();
  ext_client_init(ext_client, host, punter_ctrl);

  zl_client = zl_client_new();
  zl_client_init(zl_client, punter_ctrl);
}

void punter_forward_msg()
{
  zl_client_network_write(ext_client->ibuf, zl_client->sockfd);
}
