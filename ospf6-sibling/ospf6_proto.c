#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>

#include "ospf6_proto.h"

void 
ospf6_options_printbuf (u_char *options, char *buf, int size)
{
  const char *dc, *r, *n, *mc, *e, *v6;
  dc = (OSPF6_OPT_ISSET (options, OSPF6_OPT_DC) ? "DC" : "--");
  r  = (OSPF6_OPT_ISSET (options, OSPF6_OPT_R)  ? "R"  : "-" );
  n  = (OSPF6_OPT_ISSET (options, OSPF6_OPT_N)  ? "N"  : "-" );
  mc = (OSPF6_OPT_ISSET (options, OSPF6_OPT_MC) ? "MC" : "--");
  e  = (OSPF6_OPT_ISSET (options, OSPF6_OPT_E)  ? "E"  : "-" );
  v6 = (OSPF6_OPT_ISSET (options, OSPF6_OPT_V6) ? "V6" : "--");
  snprintf (buf, size, "%s|%s|%s|%s|%s|%s", dc, r, n, mc, e, v6);
}
