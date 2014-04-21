#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>
#include <netinet/in.h>

#include "util.h"
#include "dblist.h"
#include "prefix.h"
#include "ospf6_route.h"
#include "ospf6_top.h"
#include "ospf6_asbr.h"

int ospf6_asbr_is_asbr (struct ospf6 *o)
{   
    return o->external_table->count;
}  
