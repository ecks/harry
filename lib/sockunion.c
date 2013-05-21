#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "sockunion.h"

/* Return accepted new socket file descriptor. */
int sockunion_accept (int sock, union sockunion *su)
{
  socklen_t len;
  int client_sock;

  len = sizeof (union sockunion);
  client_sock = accept (sock, (struct sockaddr *) su, &len);

  return client_sock;
}
