#include "config.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <netinet/in.h>

#include "util.h"
#include "dblist.h"
#include "prefix.h"
#include "sisis.h"
#include "sisis_process_types.h"

typedef unsigned char byte;

int main()
{
  int arg1;

  char result[INET6_ADDRSTRLEN+1];
  byte buff[100];

  int len;
  uint64_t host_num = 1;
  int sisis_fd;
  int num_of_ctrls;

  // TODO: need to change this
  if((sisis_fd = sisis_init(host_num, SISIS_PTYPE_OSPF6_SBLING) < 0))
  {
    fprintf(stderr, "sisis_init error\n");
    exit(1);
  }

  num_of_ctrls = 0;
  struct list * ctrl_addrs = get_ctrl_addrs();
  struct route_ipv6 * route_iter;
  char s_addr[INET6_ADDRSTRLEN+1];
  LIST_FOR_EACH(route_iter, struct route_ipv6, node, ctrl_addrs)
  {
    inet_ntop(AF_INET6, &route_iter->p->prefix, s_addr, INET6_ADDRSTRLEN+1);
    fprintf(stderr, "done getting ctrl addr: %s\n", s_addr);
    num_of_ctrls++;
  }

  while(read_cmd(buff) > 0)
  {
    arg1 = buff[0];

    if(arg1 == 1)
    {
      memcpy(result, s_addr, INET6_ADDRSTRLEN);
      len = strlen(result);
    }
    else
    {
      strcpy(result, "NOPE");
      len = strlen(result);;
    }

    write_cmd(result, len);
  }
}
