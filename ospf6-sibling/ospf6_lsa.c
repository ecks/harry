#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>

#include "util.h"
#include "dblist.h"
#include "thread.h"
#include "ospf6_proto.h"
#include "ospf6_lsa.h"

extern struct thread_master * master;

/* ospf6 age functions */
/* calculate birth */
static void ospf6_lsa_age_set (struct ospf6_lsa *lsa)
{
  struct timeval now; 

  assert (lsa && lsa->header);

  if (zebralite_gettime (ZEBRALITE_CLK_MONOTONIC, &now) < 0) 
    zlog_warn ("LSA: quagga_gettime failed, may fail LSA AGEs: %s",
                                 strerror (errno));

  lsa->birth.tv_sec = now.tv_sec - ntohs (lsa->header->age);
  lsa->birth.tv_usec = now.tv_usec;

  return;
}

int
ospf6_lsa_is_changed (struct ospf6_lsa *lsa1,
                          struct ospf6_lsa *lsa2)
{
  int length;

  if (OSPF6_LSA_IS_MAXAGE (lsa1) ^ OSPF6_LSA_IS_MAXAGE (lsa2))
    return 1;
  if (ntohs (lsa1->header->length) != ntohs (lsa2->header->length))
    return 1;

  length = OSPF6_LSA_SIZE (lsa1->header) - sizeof (struct ospf6_lsa_header);
  assert (length > 0);

  return memcmp (OSPF6_LSA_HEADER_END (lsa1->header),
                 OSPF6_LSA_HEADER_END (lsa2->header), length);
}

/* RFC2328: Section 13.2 */
int
ospf6_lsa_is_differ (struct ospf6_lsa *lsa1,
                         struct ospf6_lsa *lsa2)
{
  int len; 

  assert (OSPF6_LSA_IS_SAME (lsa1, lsa2));

  /* XXX, Options ??? */

  ospf6_lsa_age_current (lsa1);
  ospf6_lsa_age_current (lsa2);
  if (ntohs (lsa1->header->age) == MAXAGE &&
      ntohs (lsa2->header->age) != MAXAGE)
    return 1;
  if (ntohs (lsa1->header->age) != MAXAGE &&
      ntohs (lsa2->header->age) == MAXAGE)
    return 1;

  /* compare body */
  if (ntohs (lsa1->header->length) != ntohs (lsa2->header->length))
    return 1;

  len = ntohs (lsa1->header->length) - sizeof (struct ospf6_lsa_header);
  return memcmp (lsa1->header + 1, lsa2->header + 1, len);
}

/* this function calculates current age from its birth,
 *    then update age field of LSA header. return value is current age */
u_int16_t
ospf6_lsa_age_current (struct ospf6_lsa *lsa)
{
  struct timeval now; 
  u_int32_t ulage;
  u_int16_t age; 

  assert (lsa);
  assert (lsa->header);

  /* curent time */
  if (zebralite_gettime (ZEBRALITE_CLK_MONOTONIC, &now) < 0)
  {
    zlog_warn("LSA: quagga_gettime failed, may fail LSA AGEs: %s",
              strerror (errno));
  }

  if (ntohs (lsa->header->age) >= MAXAGE)
  {    
    /* ospf6_lsa_premature_aging () sets age to MAXAGE; when using
       relative time, we cannot compare against lsa birth time, so
       we catch this special case here. */
    lsa->header->age = htons (MAXAGE);
    return MAXAGE;
  }    
  /* calculate age */
  ulage = now.tv_sec - lsa->birth.tv_sec;

  /* if over MAXAGE, set to it */
  age = (ulage > MAXAGE ? MAXAGE : ulage);

  lsa->header->age = htons (age);
  return age; 
}

/* update age field of LSA header with adding InfTransDelay */
void
ospf6_lsa_age_update_to_send (struct ospf6_lsa *lsa, u_int32_t transdelay)
{
  unsigned short age; 

  age = ospf6_lsa_age_current (lsa) + transdelay;
  if (age > MAXAGE)
    age = MAXAGE;
  lsa->header->age = htons (age);
}

void
ospf6_lsa_premature_aging(struct ospf6_lsa * lsa)
{
  THREAD_OFF(lsa->expire);
  THREAD_OFF(lsa->refresh);

  lsa->header->age = htons(MAXAGE);
  thread_execute(master, ospf6_lsa_expire, lsa, 0);
}

/* check which is more recent. if a is more recent, return -1;
 *    if the same, return 0; otherwise(b is more recent), return 1 */
int
ospf6_lsa_compare (struct ospf6_lsa *a, struct ospf6_lsa *b)
{
  int seqnuma, seqnumb;
  u_int16_t cksuma, cksumb;
  u_int16_t agea, ageb;

  assert (a && a->header);
  assert (b && b->header);
  assert (OSPF6_LSA_IS_SAME (a, b)); 

  seqnuma = (int) ntohl (a->header->seqnum);
  seqnumb = (int) ntohl (b->header->seqnum);

  /* compare by sequence number */
  if (seqnuma > seqnumb)
    return -1;
  if (seqnuma < seqnumb)
    return 1;

  /* Checksum */
  cksuma = ntohs (a->header->checksum);
  cksumb = ntohs (b->header->checksum);
  if (cksuma > cksumb)
    return -1;
  if (cksuma < cksumb)
    return 0;

  /* Update Age */
  agea = ospf6_lsa_age_current (a); 
  ageb = ospf6_lsa_age_current (b); 

  /* MaxAge check */
  if (agea == MAXAGE && ageb != MAXAGE)
    return -1;
  else if (agea != MAXAGE && ageb == MAXAGE)
    return 1;

  /* Age check */
  if (agea > ageb && agea - ageb >= MAX_AGE_DIFF)
    return 1;
  else if (agea < ageb && ageb - agea >= MAX_AGE_DIFF)
    return -1;

  /* neither recent */
  return 0;
}

void ospf6_lsa_header_print_raw(struct ospf6_lsa_header * header)
{
  char id[16], adv_router[16];
  inet_ntop (AF_INET, &header->id, id, sizeof (id));
  inet_ntop (AF_INET, &header->adv_router, adv_router,
                      sizeof (adv_router));
//  zlog_debug ("    [%s Id:%s Adv:%s]",
//                          ospf6_lstype_name (header->type), id, adv_router);
  zlog_debug ("    [Id:%s Adv:%s]",
                          id, adv_router);
  zlog_debug ("    Age: %4hu SeqNum: %#08lx Cksum: %04hx Len: %d",
                            ntohs (header->age), (u_long) ntohl (header->seqnum),
                                          ntohs (header->checksum), ntohs (header->length));

}

void ospf6_lsa_header_print(struct ospf6_lsa * lsa)
{
  ospf6_lsa_age_current(lsa);
  ospf6_lsa_header_print_raw(lsa->header);
}

struct ospf6_lsa *
ospf6_lsa_create_headeronly (struct ospf6_lsa_header *header)
{
  struct ospf6_lsa *lsa = NULL;
  struct ospf6_lsa_header *new_header = NULL;

  /* allocate memory for this LSA */
  new_header = (struct ospf6_lsa_header *)
               calloc(1, sizeof (struct ospf6_lsa_header));

  /* copy LSA from original header */
  memcpy (new_header, header, sizeof (struct ospf6_lsa_header));

  /* LSA information structure */
  /* allocate memory */
  lsa = (struct ospf6_lsa *)
        calloc(1, sizeof (struct ospf6_lsa));

  lsa->header = (struct ospf6_lsa_header *) new_header;
  SET_FLAG (lsa->flag, OSPF6_LSA_HEADERONLY);

  /* dump string */
//  ospf6_lsa_printbuf (lsa, lsa->name, sizeof (lsa->name));

  /* calculate birth of this lsa */
  ospf6_lsa_age_set (lsa);

  return lsa;
}

/* OSPFv3 LSA creation/deletion function */
struct ospf6_lsa * ospf6_lsa_create(struct ospf6_lsa_header * header)
{
  struct ospf6_lsa * lsa = NULL;
  struct ospf6_lsa_header * new_header = NULL;
  u_int16_t lsa_size = 0;

  /* size of the entire LSA */
  lsa_size = ntohs(header->length);

  /* allocate memory for this LSA */
  new_header = (struct ospf6_lsa_header *)calloc(1, lsa_size);

  /* copy LSA from original header */
  memcpy (new_header, header, lsa_size);

  /* LSA information structure */
  /* allocate memory */
  lsa = (struct ospf6_lsa *)calloc(1, sizeof (struct ospf6_lsa));

  lsa->header = (struct ospf6_lsa_header *) new_header;

  /* dump string */
//  ospf6_lsa_printbuf (lsa, lsa->name, sizeof (lsa->name));

  /* calculate birth of this lsa */
  ospf6_lsa_age_set (lsa);

  return lsa; 

}

void ospf6_lsa_delete(struct ospf6_lsa * lsa)
{
  assert(lsa->lock == 0);

  /* cancel threads */
  THREAD_OFF(lsa->expire);
  THREAD_OFF(lsa->refresh);

  /* do free */
  free(lsa->header);
  free(lsa);
}

struct ospf6_lsa *
ospf6_lsa_copy (struct ospf6_lsa *lsa)
{
  struct ospf6_lsa *copy = NULL;

  ospf6_lsa_age_current (lsa);
  if (CHECK_FLAG (lsa->flag, OSPF6_LSA_HEADERONLY))
    copy = ospf6_lsa_create_headeronly (lsa->header);
  else 
    copy = ospf6_lsa_create (lsa->header);
  assert (copy->lock == 0);

  copy->birth = lsa->birth;
  copy->originated = lsa->originated;
  copy->received = lsa->received;
  copy->installed = lsa->installed;
  copy->lsdb = lsa->lsdb;

  return copy;
}

/* increment reference counter of struct ospf6_lsa */
void ospf6_lsa_lock(struct ospf6_lsa * lsa)
{
  lsa->lock++;
  return;
}

/* decrement reference counter of struct ospf6_lsa */
void ospf6_lsa_unlock(struct ospf6_lsa * lsa)
{
  /* decrement reference counter */
  assert (lsa->lock > 0);
  lsa->lock--;

  if (lsa->lock != 0)
    return;

  ospf6_lsa_delete (lsa);
}

int ospf6_lsa_expire (struct thread *thread)
{
  struct ospf6_lsa *lsa;

  lsa = (struct ospf6_lsa *) THREAD_ARG (thread);

  return 0;
}

int ospf6_lsa_refresh(struct thread * thread)
{
  struct ospf6_lsa *old, *self, *new;

  old = (struct ospf6_lsa *) THREAD_ARG (thread);

  return 0;
}

/* enhanced Fletcher checksum algorithm, RFC1008 7.2 */
#define MODX                4102
#define LSA_CHECKSUM_OFFSET   15

unsigned short
ospf6_lsa_checksum (struct ospf6_lsa_header *lsa_header)
{
  u_char *sp, *ep, *p, *q;
  int c0 = 0, c1 = 0;
  int x, y;
  u_int16_t length;

  lsa_header->checksum = 0;
  length = ntohs (lsa_header->length) - 2;
  sp = (u_char *) &lsa_header->type;

  for (ep = sp + length; sp < ep; sp = q)
  {
    q = sp + MODX;
    if (q > ep)
      q = ep;
    for (p = sp; p < q; p++)
    {
      c0 += *p;
      c1 += c0;
    }
    c0 %= 255;
    c1 %= 255;
  }

  /* r = (c1 << 8) + c0; */
  x = ((length - LSA_CHECKSUM_OFFSET) * c0 - c1) % 255;
  if (x <= 0)
     x += 255;
   y = 510 - c0 - x;
   if (y > 255)
     y -= 255;

   lsa_header->checksum = htons ((x << 8) + y);

   return (lsa_header->checksum);
}

