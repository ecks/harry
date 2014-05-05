#include "config.h"

#include <stdio.h>
#include <assert.h>
#include <string.h>
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

/* When string format is invalid return 0. */
int
str2prefix_ipv4 (const char *str, struct prefix_ipv4 *p)
{
  int ret;
  int plen;
  char *pnt;
  char *cp;

  /* Find slash inside string. */
  pnt = strchr (str, '/');

  /* String doesn't contail slash. */
  if (pnt == NULL)
  {
    /* Convert string to prefix. */
    ret = inet_aton (str, &p->prefix);
    if (ret == 0)
      return 0;

    /* If address doesn't contain slash we assume it host address. */
    p->family = AF_INET;
    p->prefixlen = IPV4_MAX_BITLEN;

    return ret;
  }
  else
  {
    cp = calloc (1, (pnt - str) + 1);
    strncpy (cp, str, pnt - str);
    *(cp + (pnt - str)) = '\0';
    ret = inet_aton (cp, &p->prefix);
    free(cp);

    /* Get prefix length. */
    plen = (u_char) atoi (++pnt);
    if (plen > IPV4_MAX_PREFIXLEN)
      return 0;

    p->family = AF_INET;
    p->prefixlen = plen;
  }

  return ret;
}

#ifdef HAVE_IPV6
/* If given string is valid return pin6 else return NULL */
int
str2prefix_ipv6 (const char *str, struct prefix_ipv6 *p)
{
  char *pnt;
  char *cp;
  int ret;

  pnt = strchr (str, '/');

  /* If string doesn't contain `/' treat it as host route. */
  if (pnt == NULL)
  {
    ret = inet_pton (AF_INET6, str, &p->prefix);
    if (ret == 0)
      return 0;
    p->prefixlen = IPV6_MAX_BITLEN;
  }
  else
  {
    int plen;

    cp = calloc(1, (pnt - str) + 1);
    strncpy (cp, str, pnt - str);
    *(cp + (pnt - str)) = '\0';
    ret = inet_pton (AF_INET6, cp, &p->prefix);
    free (cp);
    if (ret == 0)
      return 0;
    plen = (u_char) atoi (++pnt);
    if (plen > 128)
      return 0;
    p->prefixlen = plen;
  }
  p->family = AF_INET6;

  return ret;
}
#endif /* HAVE_IPV6 */

/* Generic function for conversion string to struct prefix. */
int
str2prefix (const char *str, struct prefix *p) 
{
  int ret;

  /* First we try to convert string to struct prefix_ipv4. */
  ret = str2prefix_ipv4 (str, (struct prefix_ipv4 *) p); 
  if (ret)
    return ret;

#ifdef HAVE_IPV6
  /* Next we try to convert string to struct prefix_ipv6. */
  ret = str2prefix_ipv6 (str, (struct prefix_ipv6 *) p); 
  if (ret)
    return ret;
#endif /* HAVE_IPV6 */

  return 0;
}

int
prefix2str (const struct prefix *p, char *str, int size)
{
  char buf[BUFSIZ];

  inet_ntop (p->family, &p->u.prefix, buf, BUFSIZ);
  snprintf (str, size, "%s/%d", buf, p->prefixlen);
  return 0;
}

struct prefix * prefix_new()
{
  return calloc(1, sizeof(struct prefix));
}

void prefix_free(struct prefix * p)
{
  free(p);
}

struct prefix_ipv4 * prefix_ipv4_new()
{
  return calloc(1, sizeof(struct prefix_ipv4));
}

#ifdef HAVE_IPV6
struct prefix_ipv6 * prefix_ipv6_new()
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
void apply_mask_ipv4 (struct prefix_ipv4 *p)
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

void apply_mask_ipv6(struct prefix_ipv6 * p)
{
  u_char *pnt;
  int index;
  int offset;

  index = p->prefixlen / 8;

  if (index < 16) 
  {   
    pnt = (u_char *) &p->prefix;
    offset = p->prefixlen % 8;

    pnt[index] &= maskbit[offset];
    index++;

    while (index < 16) 
      pnt[index++] = 0;
  }  
}

void apply_mask(struct prefix * p)
{
  switch(p->family)
  {
    case AF_INET:
      apply_mask_ipv4((struct prefix_ipv4 *)p);
      break;
#ifdef HAVE_IPV6
    case AF_INET6:
      apply_mask_ipv6((struct prefix_ipv6 *)p);
      break;
#endif
  default:
    break;
  }
  return;
}

struct route_ipv4 * new_route()
{
  return calloc(1, sizeof(struct route_ipv4));
}
