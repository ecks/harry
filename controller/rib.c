#include "config.h"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "dblist.h"
#include "prefix.h"
#include "table.h"
#include "vector.h"
#include "rib.h"

static vector vrf_vector;

/* Allocate new VRF.  */
static struct vrf * vrf_alloc (const char *name)
{
  struct vrf *vrf;

  vrf = calloc(1, sizeof (*vrf));

  /* Put name.  */
  if (name)
    vrf->name =  strdup(name);

  /* Allocate routing table and static table.  */
  vrf->table[AFI_IP][SAFI_UNICAST] = route_table_init ();
  vrf->table[AFI_IP6][SAFI_UNICAST] = route_table_init ();
  vrf->stable[AFI_IP][SAFI_UNICAST] = route_table_init ();
  vrf->stable[AFI_IP6][SAFI_UNICAST] = route_table_init ();
  vrf->table[AFI_IP][SAFI_MULTICAST] = route_table_init ();
  vrf->table[AFI_IP6][SAFI_MULTICAST] = route_table_init ();
  vrf->stable[AFI_IP][SAFI_MULTICAST] = route_table_init ();
  vrf->stable[AFI_IP6][SAFI_MULTICAST] = route_table_init ();


  return vrf;
}

struct vrf *
vrf_lookup (u_int32_t id)
{
  return vector_lookup (vrf_vector, id);
}

static void vrf_init()
{
  struct vrf * default_table;

  /* Allocate VRF vector.  */
  vrf_vector = vector_init (1);

  /* Allocate default main table.  */
  default_table = vrf_alloc ("Default-IP-Routing-Table");

  /* Default table index must be 0.  */
  vector_set_index (vrf_vector, 0, default_table);
}

/* Lookup route table.  */
struct route_table *
vrf_table (afi_t afi, safi_t safi, u_int32_t id)
{
  struct vrf *vrf;

  vrf = vrf_lookup (id);
  if (! vrf)
    return NULL;

  return vrf->table[afi][safi];
}

/* Lookup static route table.  */
struct route_table *
vrf_static_table (afi_t afi, safi_t safi, u_int32_t id)
{
  struct vrf *vrf;

  vrf = vrf_lookup (id);
  if (! vrf)
    return NULL;

  return vrf->stable[afi][safi];
}

//struct rib *
//rib_match_ipv4 (struct in_addr addr)
//{
//  struct prefix_ipv4 p;
//  struct route_table *table;
//  struct route_node *rn;
//  struct rib *match;
//  struct nexthop *newhop;

  /* Lookup table.  */
//  table = vrf_table (AFI_IP, SAFI_UNICAST, 0);
//  if (! table)
//    return 0;

//  memset (&p, 0, sizeof (struct prefix_ipv4));
//  p.family = AF_INET;
//  p.prefixlen = IPV4_MAX_PREFIXLEN;
//  p.prefix = addr;

//  rn = route_node_match (table, (struct prefix *) &p);
//}

struct rib *
rib_lookup_ipv4 (struct prefix_ipv4 *p)
{
  struct rib *rib;
  struct route_table *table;
  struct route_node *rn;
  struct rib *match;
  struct nexthop *nexthop;

  /* Lookup table.  */
  table = vrf_table (AFI_IP, SAFI_UNICAST, 0);
  if (! table)
    return 0;

  rn = route_node_lookup (table, (struct prefix *) p);

  /* No route for this prefix. */
  if (! rn)
    return NULL;

  /* Unlock node. */
  route_unlock_node (rn);

  for (match = rn->info; match; match = match->next)
    {
//      if (CHECK_FLAG (match->status, RIB_ENTRY_REMOVED))
//        continue;
//      if (CHECK_FLAG (match->flags, ZEBRA_FLAG_SELECTED))
//        break;
    }

//  if (! match || match->type == ZEBRA_ROUTE_BGP)
//    return NULL;

//  if (match->type == ZEBRA_ROUTE_CONNECT)
//    return match;

//  for (nexthop = match->nexthop; nexthop; nexthop = nexthop->next)
//    if (CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB))
//      return match;

  return NULL;
}

/* Add RIB to head of the route node. */
static void
rib_link (struct route_node *rn, struct rib *rib)
{
  struct rib *head;
  char buf[INET6_ADDRSTRLEN];

  assert (rib && rn);

  route_lock_node (rn); /* rn route table reference */

  head = rn->info;
  if (head)
  {
      head->prev = rib;
      /* Transfer the rn status flags to the new head RIB */
      rib->rn_status = head->rn_status;
  }

  rib->next = head;
  rn->info = rib;
//  rib_queue_add (&zebrad, rn); what is this for?
}

static void
rib_addnode (struct route_node *rn, struct rib *rib)
{
  rib_link (rn, rib);
}

int rib_add_ipv4(struct route_ipv4 * route, safi_t safi)
{
  struct rib * rib;
  struct route_table *table;
  struct route_node * rn;

  /* Lookup table.  */
  table = vrf_table (AFI_IP, safi, 0);
  if (! table)
    return 0;

  apply_mask_ipv4(route->p);

  /* Set default distance by route type. */
  if (route->distance == 0)
  {
    // TODO
  }

  rn = route_node_get(table, (struct prefix *)route->p);

  /* If same type of route are installed, treat it as a implicit
     withdraw. */
  for (rib = rn->info; rib; rib = rib->next)
  {
    // TODO
  }

  /* Allocate new rib structure. */
  rib = calloc (1, sizeof (struct rib));
  rib->type = route->type;
  rib->distance = route->distance;
  rib->flags = route->flags;
  rib->metric = route->metric;
  rib->table = route->vrf_id;
//  rib->nexthop_num = 0;
  rib->uptime = time (NULL);

  // TODO: Nexthop settings

  /* Link new rib to node.*/
  rib_addnode (rn, rib);

  route_unlock_node (rn);
  return 0;
}

void rib_init()
{
  vrf_init();  
}
