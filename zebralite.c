#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <syslog.h>
#include <netinet/in.h>

#include "lib/dblist.h"
#include "api.h"
#include "thread.h"
#include "log.h"
#include "vty.h"
#include "vector.h"
#include "command.h"
#include "lib/stream.h"
#include "listener.h"

#define DEFAULT_CONFIG_FILE "zebralite.conf"

struct thread_master * master;

/* Default configuration file path. */
char config_default[] = SYSCONFDIR DEFAULT_CONFIG_FILE;

int main(int argc, char ** argv)
{
  char * config_file = "/etc/zebralite/zebralite.conf";
  
  struct thread thread;
  int i;

  if(argc < 1)
  {
    fprintf(stderr, "./zebralite protocol:::address\n");
    exit(1);
  }

  zlog_default = openzlog(argv[0], ZLOG_ZEBRALITE, LOG_CONS|LOG_NDELAY|LOG_PID, LOG_DAEMON);

  cmd_init(1);
  vty_init(master);

  zebra_debug_init();
  listener_init();

  vty_read_config(config_file, config_default);


  /* thread master */
  master = thread_master_create();

  api_init();

  listener_run();


  zlog_notice("Zebralite starting");

  // spin forever
  while(thread_fetch(master, &thread))
  {
    thread_call(&thread);
  }
}
