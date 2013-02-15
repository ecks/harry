#include "config.h"

#include "unistd.h"
#include "netdb.h"
#include "stdbool.h"
#include "stdio.h"
#include "stdlib.h"

#include "routeflow-common.h"
#include "socket-util.h"
#include "dblist.h"
#include "api.h"
#include "thread.h"
#include "punter_ctrl.h"
#include "punter.h"

struct thread_master * master;

int main(int argc, char **argv)
{
  struct thread thread;
  struct addrinfo *ai;

  if(optind != argc-1)
  {
    printf("usage: listener <hostname>\n");
    exit(1);
  }
  
  /* thread master */
  master = thread_master_create();

  host = argv[optind];
 
  api_init();
 
  punter_ctrl_init(host);

  while(thread_fetch(master, &thread))
  {
    thread_call(&thread);
  }

}
