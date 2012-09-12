#ifndef STREAM_FD_H
#define STREAM_FD_H 1

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct stream;
struct pstream;
struct sockaddr;

int new_fd_stream(const char *name, int fd, int connect_status,
                  struct stream **streamp);

#endif
