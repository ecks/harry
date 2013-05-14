#include "ext_client_provider.h"

void ext_client_bgp_init(struct ext_client * ext_client)
{

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
