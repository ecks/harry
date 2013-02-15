#include "stdlib.h"

#include "zl_client.h"

struct zl_client * zl_client_new()
{
  return calloc(1, sizeof(struct zl_client));
}

void zl_client_init(struct zl_client * zl_client)
{

}
