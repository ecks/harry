#ifndef ROUTEV4_H
#define ROUTEv4_H

#include <stdarg.h>
#include "Class.h"

struct RouteV4
{
  const void * class;
  struct route_ipv4 * route;
};

void * RouteV4_ctor(void * _self, va_list * app);
void RouteV4_xtor(void * _self, va_list * app);
bool RouteV4_cmp(void * _self, void * _b, va_list * app);
void * RouteV4_dup(void * _self);
void * RouteV4_dtor(void * _self);

static const struct Class _RouteV4 = 
{
  sizeof(struct RouteV4), 
  RouteV4_ctor,
  RouteV4_xtor,
  RouteV4_cmp,
  RouteV4_dup,
  RouteV4_dtor
};

static const void * RouteV4 = &_RouteV4;

#endif
