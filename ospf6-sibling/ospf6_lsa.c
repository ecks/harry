#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "util.h"
#include "dblist.h"
#include "thread.h"
#include "ospf6_proto.h"
#include "ospf6_lsa.h"

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
