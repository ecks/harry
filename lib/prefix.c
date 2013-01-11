#include "config.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "dblist.h"
#include "prefix.h"

/* Maskbit. */
static const u_char maskbit[] = {0x00, 0x80, 0xc0, 0xe0, 0xf0,
                                 0xf8, 0xfc, 0xfe, 0xff};

/* Number of bits in prefix type. */
#ifndef PNBBY
#define PNBBY 8
#endif /* PNBBY */

struct prefix * new_prefix()
{
  return calloc(1, sizeof(struct prefix));
}

struct prefix_ipv4 * new_prefix_v4()
{
  return calloc(1, sizeof(struct prefix_ipv4));
}

#ifdef HAVE_IPV6
struct prefix_ipv6 * new_prefix_v6()
{
  return calloc(1, sizeof(struct prefix_ipv6));
}
#endif 

/* If n includes p prefix then return 1 else return 0. */
int 
prefix_match (const struct prefix *n, const struct prefix *p)
{
  int offset;  int shift;
  /* Set both prefix's head pointer. */  const u_char *np = (const u_char *)&n->u.prefix;  const u_char *pp = (const u_char *)&p->u.prefix;

  /* If n's prefix is longer than p's one return 0. */
  if (n->prefixlen > p->prefixlen)
    return 0;
  offset = n->prefixlen / PNBBY;  shift =  n->prefixlen % PNBBY;

  if (shift)
    if (maskbit[shift] & (np[offset] ^ pp[offset]))
      return 0;

  while (offset--)
    if (np[offset] != pp[offset])
      return 0;
  return 1;
}

/* 
 * Return 1 if the address/netmask contained in the prefix structure
 * is the same, and else return 0.  For this routine, 'same' requires
 * that not only the prefix length and the network part be the same,
 * but also the host part.  Thus, 10.0.0.1/8 and 10.0.0.2/8 are not
 * the same.  Note that this routine has the same return value sense
 * as '==' (which is different from prefix_cmp).
 */
int
prefix_same (struct prefix *p1, struct prefix *p2)
{
  if (p1->family == p2->family && p1->prefixlen == p2->prefixlen)
    {
      if (p1->family == AF_INET)
        if (IPV4_ADDR_SAME (&p1->u.prefix, &p2->u.prefix))
          return 1;
#ifdef HAVE_IPV6
      if (p1->family == AF_INET6 )
        if (IPV6_ADDR_SAME (&p1->u.prefix, &p2->u.prefix))
          return 1;
#endif /* HAVE_IPV6 */
    }
  return 0;
}

/* Copy prefix from src to dest. */
void
prefix_copy (struct prefix *dest, const struct prefix *src)
{
  dest->family = src->family;
  dest->prefixlen = src->prefixlen;

  if (src->family == AF_INET)
    dest->u.prefix4 = src->u.prefix4;
#ifdef HAVE_IPV6
  else if (src->family == AF_INET6)
    dest->u.prefix6 = src->u.prefix6;
#endif /* HAVE_IPV6 */
  else if (src->family == AF_UNSPEC)
    {
      dest->u.lp.id = src->u.lp.id;
      dest->u.lp.adv_router = src->u.lp.adv_router;
    }
  else
    {
       printf("prefix_copy(): Unknown address family %d",
              src->family);
      assert (0);
    }
}

/* Convert IP address's netmask into integer. We assume netmask is
   sequential one. Argument netmask should be network byte order. */
u_char
ip_masklen (struct in_addr netmask)
{
  u_char len;
  u_char *pnt;
  u_char *end;
  u_char val;

  len = 0;
  pnt = (u_char *) &netmask;
  end = pnt + 4;

  while ((pnt < end) && (*pnt == 0xff))
    {
      len+= 8;
      pnt++;
    }

  if (pnt < end)
    {
      val = *pnt;
      while (val)
        {
          len++;
          val <<= 1;
        }
    }
  return len;
}

/* Apply mask to IPv4 prefix. */
void
apply_mask_ipv4 (struct prefix_ipv4 *p)
{
  u_char *pnt;
  int index;
  int offset;

  index = p->prefixlen / 8;

  if (index < 4)
    {
      pnt = (u_char *) &p->prefix;
      offset = p->prefixlen % 8;

      pnt[index] &= maskbit[offset];
      index++;

      while (index < 4)
        pnt[index++] = 0;
    }
}

struct route_ipv4 * new_route()
{
  return calloc(1, sizeof(struct route_ipv4));
}
