#include "stdio.h"
#include "stdbool.h"
#include "assert.h"

#include "util.h"
#include "ext_client_provider.h"

static struct ext_client_class * ext_client_classes[] = {
  &ospf6_ext_client_class,
  &bgp_ext_client_class,
};

void ext_client_init(struct ext_client * ext_client, struct ext_client_class * ext_client_class)
{
  int i;

  for(i = 0; i < ARRAY_SIZE(ext_client_classes); i++)
  {
    assert (ext_client_classes[i]->init != NULL);
  }
}
