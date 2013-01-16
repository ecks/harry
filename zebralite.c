#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <netinet/in.h>

#include "lib/dblist.h"
#include "api.h"
#include "lib/stream.h"
#include "listener.h"

int main(int argc, char ** argv)
{
  int i;

  api_init();

  if(argc < 1)
  {
    fprintf(stderr, "At least one argument required\n");
    exit(1);
  }

  for(i = 1; i < argc; i++) // ignore program name for now
  {
    const char * name;
    name = argv[i];
    listener_init(name);
  }

//  listener_read(stream, STREAM_LEN);

  // spin forever
  while(1)
  {
  }
}
