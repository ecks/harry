#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <assert.h>
#include "netinet/in.h"
#include "dblist.h"
#include "ip.h"

#include "RouteV4.h"

void * RouteV4_ctor(void * _self, va_list * app)
{
  struct RouteV4 * self = _self;
  struct route_ipv4 * route = va_arg(*app, struct route_ipv4 *);

  self->route = calloc(1, sizeof(struct route_ipv4));
  assert(self->route);

  memcpy(self->route, route, sizeof(struct route_ipv4));
 
  return self;
}

void RouteV4_xtor(void * _self, va_list * app)
{
  struct RouteV4 * self = _self;
  struct route_ipv4 ** route = va_arg(*app, struct route_ipv4 **);
 
  *route = self->route;
}

bool RouteV4_cmp(void * _self, void * _b, va_list * app)
{

}

void * RouteV4_dup(void * _self)
{

}

void * RouteV4_dtor(void * _self)
{
  struct RouteV4 * self = _self;

  free(self->route);
  self->route = NULL;

  return self;
}
