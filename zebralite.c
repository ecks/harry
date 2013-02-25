#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <netinet/in.h>

#include "lib/dblist.h"
#include "api.h"
#include "thread.h"
#include "lib/stream.h"
#include "listener.h"

struct thread_master * master;

int main(int argc, char ** argv)
{
  struct thread thread;
  int i;

  if(argc < 1)
  {
    fprintf(stderr, "./zebralite protocol:::address\n");
    exit(1);
  }

  /* thread master */
  master = thread_master_create();

  api_init();

  const char * name;
  name = argv[1];
  listener_init(name);


  // spin forever
  while(thread_fetch(master, &thread))
  {
    thread_call(&thread);
  }
}
