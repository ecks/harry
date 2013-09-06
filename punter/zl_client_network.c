#include <stdio.h>
#include <stddef.h>
#include <sys/un.h>
#include <stdbool.h>
#include <sys/socket.h>

#include "util.h"
#include "dblist.h"
#include "rfpbuf.h"
#include "zl_client_network.h"

int zl_client_sock_init(const char * path)
{
  int ret;
  int sock,len;
  struct sockaddr_un addr;

  if((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
  {
    return -1;
  }
  memset(&addr, 0, sizeof(struct sockaddr_un));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, path, strlen(path));
#ifdef HAVE_STRUCT_SOCKADDR_UN_SUN_LEN
  len = addr.sun_len = SUN_LEN(&addr);
#else
  len = sizeof (addr.sun_family) + strlen (addr.sun_path);
#endif /* HAVE_STRUCT_SOCKADDR_UN_SUN_LEN */

  ret = connect (sock, (struct sockaddr *) &addr, len);
  if (ret < 0) 
    {    
      close (sock);
      return -1;
    }    
  return sock;
}

int zl_client_network_write(struct rfpbuf * buf, unsigned int sock)
{
  printf("Writing data to zebralite\n");

  return rfpbuf_write(buf, sock);
}

int zl_client_network_recv(struct rfpbuf * buf, unsigned int sock, unsigned int size)
{
  printf("Reading data from zebralite\n");

  return rfpbuf_read_try(buf, sock, size);
}

