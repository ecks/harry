#include "config.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <syslog.h>
#include <netinet/in.h>
#include <getopt.h>

#include "routeflow-common.h"
#include "util.h"
#include "dblist.h"
#include "if.h"
#include "prefix.h"
#include "socket-util.h"
#include "dblist.h"
#include "rfpbuf.h"
#include "rfp-msgs.h"
#include "thread.h"
#include "log.h"
#include "vty.h"
#include "vector.h"
#include "command.h"
#include "debug.h"
#include "vconn.h"
#include "ospf6_top.h"
#include "ospf6_replica.h"
#include "sisis.h"
#include "sisis_api.h"
#include "sisis_process_types.h"
#include "sibling_ctrl.h"
#include "sibling.h"

#define DEFAULT_CONFIG_FILE "ospf6-sibling.conf"

/* ospf6d options */
struct option longopts[] =
{
  { "config_file", required_argument, NULL, 'f'},
  { 0 }
};

/* Default configuration file path. */
char config_default[] = SYSCONFDIR DEFAULT_CONFIG_FILE;

struct thread_master * master;

int main(int argc, char *argv[])
{
  int opt;
  char * config_file = "/etc/zebralite/ospf6_sibling.conf";
  char * sisis_addr;
  struct in6_addr * sibling_addr;
  struct list * replicas;
  struct vconn * vconn;
  int retval;
  struct rfpbuf * buffer;
  struct thread thread;
  struct in6_addr * ctrl_addr;

  int sisis_fd;
  uint64_t host_num = 1;

  bool restart_mode = false;

  /* Command line argument treatment. */
  while(1)
  {
    opt = getopt_long(argc, argv, "rf:", longopts, 0);

    if(opt == EOF)
      break;

    switch(opt)
    {
      case 'f':
        config_file = optarg;
        break;

      case 'r':
        restart_mode = true;
        break;
    }
  }

  zlog_default = openzlog(argv[0], ZLOG_OSPF6_SIBLING, LOG_CONS|LOG_NDELAY|LOG_PID, LOG_DAEMON);
  
  cmd_init(1);
  vty_init(master);

  ospf6_sibling_debug_init();

  // TODO: signal_init

  /* thread master */
  master = thread_master_create();

  // initialize our interface list
  if_init();

  /* initialize ospf6 */
  ospf6_top_init(restart_mode);
  ospf6_area_init();

  sisis_addr = calloc(INET6_ADDRSTRLEN, sizeof(char));
  if((sisis_fd = sisis_init(sisis_addr, host_num, SISIS_PTYPE_OSPF6_SBLING) < 0))
  {
    printf("sisis_init error\n");
    exit(1);
  }

  sibling_addr = calloc(1, sizeof(struct in6_addr));

  inet_pton(AF_INET6, sisis_addr, sibling_addr);

  replicas = get_ospf6_sibling_addrs();

  ospf6_replicas_init(sibling_addr, replicas);

  // this is where the command actually gets executed
  vty_read_config(config_file, config_default);

  if(restart_mode)
  {
    zlog_notice("<---- OSPF6 Sibling starting in restart mode: %d ---->", getpid());
  }
  else
  {
    zlog_notice("<---- OSPF6 Sibling starting in normal mode: %d ---->", getpid());
  }

  zlog_debug("sibling sisis addr: %s", sisis_addr);

  free(sisis_addr);

  unsigned int num_of_controllers = number_of_sisis_addrs_for_process_type(SISIS_PTYPE_CTRL);

  if(IS_OSPF6_SIBLING_DEBUG_SISIS)
  {
    zlog_debug("num of controllers: %d", num_of_controllers);
  }
 
  struct list * ctrl_addrs = get_ctrl_addrs();
  struct route_ipv6 * route_iter;
  LIST_FOR_EACH(route_iter, struct route_ipv6, node, ctrl_addrs)
  {
    
    char s_addr[INET6_ADDRSTRLEN+1];
    inet_ntop(AF_INET6, &route_iter->p->prefix, s_addr, INET6_ADDRSTRLEN+1);
    zlog_debug("done getting ctrl addr: %s", s_addr);

    struct in6_addr * ctrl_addr = calloc(1, sizeof(struct in6_addr));
    memcpy(ctrl_addr, &route_iter->p->prefix, sizeof(struct in6_addr));
    sibling_ctrl_init(ctrl_addr, sibling_addr);
  }


  // free  the list, no longer needed
  while(!list_empty(ctrl_addrs))
  {
    struct list * addr_to_remove = list_pop_front(ctrl_addrs);
    struct route_ipv6 * route_to_remove = CONTAINER_OF(addr_to_remove, struct route_ipv6, node);
    free(route_to_remove->p);
    free(route_to_remove->gate);
  }

  free(ctrl_addrs);
  
  // Monitor rib changes in the case that we need to restart it
  struct subscribe_to_rib_changes_info info;
  info.rib_add_ipv4_route = rib_monitor_add_ipv4_route;
  info.rib_remove_ipv4_route = rib_monitor_remove_ipv4_route;
  info.rib_add_ipv6_route = rib_monitor_add_ipv6_route;
  info.rib_remove_ipv6_route = rib_monitor_remove_ipv6_route;
  subscribe_to_rib_changes(&info);

  /* Start finite state machine, here we go! */
  while(thread_fetch(master, &thread))
    thread_call(&thread);

}
