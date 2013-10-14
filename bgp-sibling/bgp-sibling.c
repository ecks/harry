#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <syslog.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <getopt.h>

#include "util.h"
#include "dblist.h"
#include "routeflow-common.h"
#include "if.h"
#include "thread.h"
#include "log.h"
#include "prefix.h"
#include "sisis.h"
#include "sisis_process_types.h"

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

int main(int argc, char * argv[])
{
  int opt;
  char * config_file = "/etc/zebralite/bgp_sibling.conf";
  char * sisis_addr;
  int sisis_fd;
  uint64_t host_num = 1;

  struct thread thread;

  zlog_default = openzlog(argv[0], ZLOG_BGP_SIBLING, LOG_CONS|LOG_NDELAY|LOG_PID, LOG_DAEMON);

  /* Command line argument treatment. */
  while(1)
  {
    opt = getopt_long(argc, argv, "f:i:", longopts, 0);

    if(opt == EOF)
      break;

    switch(opt)
    {
      case 'f':
        config_file = optarg;
        break;
    }
  }

  cmd_init(1);
  vty_init(master);


  /* thread master */
  master = thread_master_create();

  // initialize interface list
  if_init();

  // this is where the command actually gets executed
  vty_read_config(config_file, config_default);
   
  zlog_notice("<----BGP Sibling starting: %d ---->", getpid());
  
  sisis_addr = calloc(INET6_ADDRSTRLEN, sizeof(char));
  if((sisis_fd = sisis_init(sisis_addr, host_num, SISIS_PTYPE_BGP_SBLING)) < 0)
  {
    printf("sisis_init error\n");
    exit(1);
  }

  struct list * ctrl_addrs = get_ctrl_addrs();
  struct route_ipv6 * route_iter;
  LIST_FOR_EACH(route_iter, struct route_ipv6, node, ctrl_addrs)
  {
    char s_addr[INET6_ADDRSTRLEN+1];
    inet_ntop(AF_INET6, &route_iter->p->prefix, s_addr, INET6_ADDRSTRLEN+1);
    zlog_debug("done getting ctrl addr: %s\n", s_addr);

    struct in6_addr * ctrl_addr = calloc(1, sizeof(struct in6_addr));
    memcpy(ctrl_addr, &route_iter->p->prefix, sizeof(struct in6_addr));
    bgp_sibling_ctrl_init(ctrl_addr);
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

  /* Start finite state machine, here we go! */
  while(thread_fetch(master, &thread))
    thread_call(&thread);
}
