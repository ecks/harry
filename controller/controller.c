#include "config.h"

#include "stdio.h"
#include "stdint.h"
#include "stdbool.h"
#include "stdlib.h"
#include "stddef.h"
#include <syslog.h>
#include "errno.h"

#include "util.h"
#include "socket-util.h"
#include "routeflow-common.h"
#include "dblist.h"
#include "thread.h"
#include "log.h"
#include "vty.h"
#include "vector.h"
#include "command.h"
#include "rconn.h"
#include "riack.h"
#include "db.h"
#include "router.h"
#include "sib_router.h"
#include "rfpbuf.h"
#include "vconn.h"
#include "sisis.h"
#include "sisis_process_types.h"

#define MAX_ROUTERS 16
#define MAX_LISTENERS 16

#define DEFAULT_CONFIG_FILE "controller.conf"

/* Default configuration file path. */
char config_default[] = SYSCONFDIR DEFAULT_CONFIG_FILE;

struct thread_master * master;

struct sib_router_
{
  struct sib_router * sib_router;
};

struct router_
{
  struct router * router;
};

static struct router * routers[MAX_ROUTERS];
int n_routers;

//static struct sib_router * siblings[MAX_SIBLINGS];
//int n_siblings;

static int nb_listener_accept(struct thread * t);

static void
new_router(struct router **, struct vconn *, const char *);

static int sib_listener_accept(struct thread * t);

static void
new_sib_router(struct sib_router **, struct vconn *, const char *, struct router **, int *);

uint64_t hostnum;
uint64_t num_sibs_conf;

int voter_type;

riack_client * r_client = NULL;

DEFUN(hostnumber,
      hostnumber_cmd,
      "hostnum NUM",
      "Set hostnumber controller should reside on\n")
{
  hostnum = (uint64_t)strtol(argv[0], NULL, 10);

  return CMD_SUCCESS;
}

DEFUN(riack_host,
      riack_host_cmd,
      "riack host A.B.C.D",
      "Riack host Address\n"
      V4NOTATION_STR)
{
  int port = 8087;

  r_client = db_init((char *)argv[0], port);
}

DEFUN(num_sibs,
      num_sibs_cmd,
      "num sibs NUM",
      "numbe of siblings necessary\n")
{
  num_sibs_conf = (uint64_t)strtol(argv[0], NULL, 10);  
}

DEFUN(voter_all,
      voter_all_cmd,
      "voter all",
      "votes on all siblings")
{
  voter_type = VOTER_ALL;
}

DEFUN(voter_any,
      voter_any_cmd,
      "voter any",
      "votes on any siblings that are present")
{
  voter_type = VOTER_ANY;
}

int main(int argc, char *argv[])
{
  char * config_file = "/etc/zebralite/controller.conf";

  struct thread thread;
  int n_nb_listeners, n_sib_ospf6_listeners, n_sib_bgp_listeners;
  struct pvconn * nb_listeners[MAX_LISTENERS];
  struct pvconn * nb_pvconn;   // north-bound interface connection
  struct pvconn * sib_pvconn;  // sibling channel interface
  char * sisis_addr;

  int retval;
  int i;

  n_nb_listeners = 0;
  n_routers = 0;

  // TODO: set up signal handlers
//  signal(SIGABRT, );
//  signal(SIGTERM, );
//  signal(SIGINT, );

  zlog_default = openzlog(argv[0], ZLOG_CONTROLLER, LOG_CONS|LOG_NDELAY|LOG_PID, LOG_DAEMON);

  cmd_init(1);
  vty_init(master);

  controller_debug_init();
  install_element(CONFIG_NODE, &hostnumber_cmd);
  install_element(CONFIG_NODE, &riack_host_cmd);
  install_element(CONFIG_NODE, &num_sibs_cmd);
  install_element(CONFIG_NODE, &voter_any_cmd);
  install_element(CONFIG_NODE, &voter_all_cmd);
  vty_read_config(config_file, config_default);

  zlog_notice("<---- Controller starting: %d ---->", getpid());
    
  /* thread master */
  master = thread_master_create();

  time_init();
  rib_init();

  // initialize internal socket to sisis
  int sisis_fd;

  sisis_addr = calloc(INET6_ADDRSTRLEN, sizeof(char));

  if((sisis_fd = sisis_init(sisis_addr, hostnum, SISIS_PTYPE_CTRL)) < 0)
  {
    printf("sisis_init error\n");
    exit(1);
  }
  
  retval = pvconn_open("ptcp6:6633", &nb_pvconn, DSCP_DEFAULT);
  if(!retval)
  {
    if(n_nb_listeners >= MAX_LISTENERS)
    {
      printf("max switch %d connections\n", n_nb_listeners);
    }
    nb_listeners[n_nb_listeners++] = nb_pvconn;
  }

  sib_router_init(routers, &n_routers, num_sibs_conf, voter_type, r_client);

  if(n_routers < MAX_ROUTERS)
  {
    for(i = 0; i < n_nb_listeners; i++)
    {
      pvconn_wait(nb_listeners[i], nb_listener_accept, nb_listeners[i]);
    }
  }

  // this is ineffective since there are no siblings yet
//  for(i = 0; i < n_ospf6_siblings; i++)
//  {
//    struct sib_ospf6_router * sib_router = siblings[i];
//    sib_router_wait(sib_router);
//  }

  // this is ineffective since there are no routers yet
//  for(i = 0; i < n_routers; i++)
//  {
//    struct router * router = routers[i];
//    router_wait(router);
//  }

//    poll_block();
 
  zlog_notice("Controller starting");

  // spin forever
  while(thread_fetch(master, &thread))
  {
    thread_call(&thread);
  }
 
  return 0;
}

static int nb_listener_accept(struct thread * t)
{
  struct pvconn * nb_listener = THREAD_ARG(t);
  struct vconn * new_nb_vconn;
  int retval;

  retval = pvconn_accept(nb_listener, RFP10_VERSION, &new_nb_vconn);
  if(!retval)
  {
    printf("new router connection\n");
    new_router(&routers[n_routers++], new_nb_vconn, "tcp6");
  }
  else if(retval != EAGAIN)
  {
    pvconn_close(nb_listener);
  }

  // reregister ourselves
  pvconn_wait(nb_listener, nb_listener_accept, nb_listener);

  return 0;
}


static void
new_router(struct router ** rt, struct vconn *vconn, const char *name)
{
    struct rconn * rconn;
  
    rconn = rconn_create();
    rconn_connect_unreliably(rconn, vconn, NULL);
    *rt = router_create(rconn, r_client);

    // schedule an event to call router_run
    thread_add_event(master, router_run, *rt, 0);
}
