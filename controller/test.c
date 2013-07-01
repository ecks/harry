#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#define PORT 9999
int main()
{
  struct sockaddr_in sin;
  int accept_fd;
  int error;
  int yes=1;

  struct sockaddr_in client;
  int client_fd;
  socklen_t len;

  accept_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (accept_fd < 0) {
    error = errno;
    printf("socket: %s\n", strerror(error));
    goto error;
  }

  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = htonl(INADDR_ANY);
  sin.sin_port = htons(PORT);

  if (setsockopt(accept_fd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) 
  {
    perror("setsockopt");
    goto error;
  }

  if(bind(accept_fd, (struct sockaddr *)&sin, sizeof sin) < 0)
  {
    error = errno;
    printf("bind : %s\n", strerror(error));
    goto error;
  }

  if(listen(accept_fd, 10) < 0)
  {
    error = errno;
    printf("listen : %s\n", strerror(error));
    goto error;
  }

  len = sizeof client;
  client_fd = accept(accept_fd, (struct sockaddr *)&client, &len);

  printf("client fd: %d\n", client_fd);
  if(client_fd < 0)
  {
    error = errno;
    printf("accept : %s\n", strerror(error));
    goto error;
  }

error:
  close(accept_fd);
  exit(1);
}
