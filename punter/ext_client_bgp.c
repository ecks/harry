#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

#include "routeflow-common.h"
#include "ext_client_provider.h"

struct ext_client_bgp
{
  struct ext_client ext_client;
};
 
struct ext_client * ext_client_bgp_init(char * host, struct punter_ctrl * punter_ctrl)
{
  return NULL;
}

void ext_client_bgp_send(struct ext_client * ext_client)
{

}

#define EXT_CLIENT_INIT                   \
  {                                       \
    ext_client_bgp_init,                  \
    ext_client_bgp_send,                  \
  }

struct ext_client_class bgp_ext_client_class = EXT_CLIENT_INIT;
