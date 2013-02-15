#ifndef PREFIX_H
#define PREFIX_H

/* IPv4 prefix structure. */
struct prefix_ipv4
{
  u_char family;
  u_char prefixlen;
  struct in_addr prefix __attribute__ ((aligned (8)));
};

/* IPv6 prefix structure. */
#ifdef HAVE_IPV6
struct prefix_ipv6
{
  u_char family;
  u_char prefixlen;
  struct in6_addr prefix __attribute__ ((aligned (8)));
};
#endif /* HAVE_IPV6 */

/* IPv4 and IPv6 unified prefix structure. */
struct prefix
{
  u_char family;
  u_char prefixlen;
  union 
  {
    u_char prefix;
    struct in_addr prefix4;
#ifdef HAVE_IPV6
    struct in6_addr prefix6;
#endif /* HAVE_IPV6 */
    struct 
    {
      struct in_addr id;
      struct in_addr adv_router;
    } lp;
    u_char val[8];
  } u __attribute__ ((aligned (8)));
};


struct route_ipv4
{
	int type;
	int flags;
	struct prefix_ipv4 *p;
	struct in_addr *gate;
	struct in_addr *src;
	unsigned int ifindex;
	u_int32_t vrf_id;
	u_int32_t metric;
	u_char distance;
        struct list node;
};

struct addr_ipv4
{
  struct prefix_ipv4 *p;
  unsigned int ifindex;
  struct list node;
};

#ifdef HAVE_IPV6
struct route_ipv6
{
	int type;
	int flags;
	struct prefix_ipv6 *p;
	struct in_addr *gate;
	unsigned int ifindex;
	u_int32_t vrf_id;
	u_int32_t metric;
	u_char distance;
        struct list node;
};

struct addr_ipv6
{
  struct prefix_ipv6 *p;
  unsigned int ifindex;
  struct list node;
};
#endif /* HAVE_IPV6 */

#define IPV4_MAX_BYTELEN    4
#define IPV4_MAX_PREFIXLEN 32
#define IPV4_ADDR_SAME(D,S)  (memcmp ((D), (S), IPV4_MAX_BYTELEN) == 0)

#define IPV6_MAX_BYTELEN    16
#define IPV6_ADDR_SAME(D,S)  (memcmp ((D), (S), IPV6_MAX_BYTELEN) == 0)

struct prefix * new_prefix();
struct prefix_ipv4 * new_prefix_v4();
#ifdef HAVE_IPV6
struct prefix_ipv6 * new_prefix_v6();
#endif

struct route_ipv4 * new_route();

/* Check bit of the prefix. */
static inline unsigned int
prefix_bit (const u_char *prefix, const u_char prefixlen)
{
  unsigned int offset = prefixlen / 8;
  unsigned int shift  = 7 - (prefixlen % 8);

  return (prefix[offset] >> shift) & 1;
}

#endif
