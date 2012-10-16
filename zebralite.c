#include <netinet/in.h>

#include "lib/dblist.h"
#include "api.h"
#include "lib/stream.h"
#include "listener.h"

int main(int argc, char ** argv)
{
  api_init();

  listener_init();

//  listener_read(stream, STREAM_LEN);

  // spin forever
  while(1)
  {
  }
}
