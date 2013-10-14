#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "dblist.h"
#include "ospf6_lsa.h"
#include "ospf6_lsdb.h"
#include "ospf6_flood.h"

struct ospf6_lsdb * ospf6_get_scoped_lsdb(struct ospf6_lsa * lsa)
{
  struct ospf6_lsdb * lsdb = NULL;

  // not implemented for now

  return lsdb;
}


void ospf6_increment_retrans_count(struct ospf6_lsa * lsa)
{
  /* The LSA must be the original one (see the description
     in ospf6_decrement_retrans_count () below) */
  lsa->retrans_count++;
}

void
ospf6_decrement_retrans_count (struct ospf6_lsa *lsa)
{
  struct ospf6_lsdb *lsdb;
  struct ospf6_lsa *orig;

  /* The LSA must be on the retrans-list of a neighbor. It means
     the "lsa" is a copied one, and we have to decrement the
     retransmission count of the original one (instead of this "lsa"'s).
     In order to find the original LSA, first we have to find
     appropriate LSDB that have the original LSA. */
  lsdb = ospf6_get_scoped_lsdb (lsa);

  /* Find the original LSA of which the retrans_count should be decremented */
  orig = ospf6_lsdb_lookup (lsa->header->type, lsa->header->id,
                            lsa->header->adv_router, lsdb);
  if (orig)
  {    
    orig->retrans_count--;
    assert (orig->retrans_count >= 0);
  }    
}

