#ifndef RIB_H
#define RIB_H

/* Address family numbers from RFC1700. */
#define AFI_IP                    1
#define AFI_IP6                   2
#define AFI_MAX                   3
#define SAFI_MAX                  5

/* Subsequent Address Family Identifier. */
#define SAFI_UNICAST              1
#define SAFI_MULTICAST            2
#define SAFI_RESERVED_3           3
#define SAFI_MPLS_VPN             4
#define SAFI_MAX                  5

#define DISTANCE_INFINITY         255
typedef u_int16_t afi_t;
typedef u_int8_t safi_t;

struct rib
{
  /* Status Flags for the *route_node*, but kept in the head RIB.. */
  u_char rn_status;

  /* Link list. */
  struct rib *next;
  struct rib *prev;

  /* Uptime. */
  time_t uptime;

  /* Type fo this route. */
  int type;

  /* Which routing table */
  int table;

  /* Metric */
  u_int32_t metric;

  /* Distance. */
  u_char distance;

  /* Flags of this route.
   * This flag's definition is in lib/zebra.h ZEBRA_FLAG_* and is exposed
   * to clients via Zserv
   */
  u_char flags;

  /* RIB internal status */
  u_char status;
};

/* Routing table instance.  */
struct vrf
{
  /* Identifier.  This is same as routing table vector index.  */
  u_int32_t id;

  /* Routing table name.  */
  char *name;

  /* Description.  */
  char *desc;

  /* FIB identifier.  */
  u_char fib_id;

  /* Routing table.  */
  struct route_table *table[AFI_MAX][SAFI_MAX];

  /* Static route configuration.  */
  struct route_table *stable[AFI_MAX][SAFI_MAX];
};

struct vrf * vrf_lookup (u_int32_t id);
struct route_table * vrf_table (afi_t afi, safi_t safi, u_int32_t id);
struct route_table * vrf_static_table (afi_t afi, safi_t safi, u_int32_t id);
struct rib * rib_lookup_ipv4 (struct prefix_ipv4 *p);
int rib_add_ipv4(struct route_ipv4 * route, safi_t safi);
void rib_init();

#endif
