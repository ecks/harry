#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "dblist.h"
#include "thread.h"
#include "routeflow-common.h"
#include "ospf6_proto.h"
#include "ospf6_lsa.h"
#include "ospf6_lsdb.h"
#include "ospf6_interface.h"
#include "ospf6_top.h"
#include "ospf6_neighbor.h"
#include "ospf6_flood.h"

extern struct thread_master * master;

struct ospf6_lsdb * ospf6_get_scoped_lsdb(struct ospf6_lsa * lsa)
{
  struct ospf6_lsdb * lsdb = NULL;

  // not implemented for now

  return lsdb;
}

struct ospf6_lsdb * ospf6_get_scoped_lsdb_self(struct ospf6_lsa * lsa)
{
  struct ospf6_lsdb * lsdb_self = NULL;
  switch (OSPF6_LSA_SCOPE (lsa->header->type))
  {    
    case OSPF6_SCOPE_LINKLOCAL:
      lsdb_self = OSPF6_INTERFACE (lsa->lsdb->data)->lsdb_self;
      break;
    case OSPF6_SCOPE_AREA:
//      lsdb_self = OSPF6_AREA (lsa->lsdb->data)->lsdb_self;
      break;
    case OSPF6_SCOPE_AS:
//      lsdb_self = OSPF6_PROCESS (lsa->lsdb->data)->lsdb_self;
      break;
    default:
      assert (0); 
      break;
  }    
  return lsdb_self;
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

void 
ospf6_lsa_originate(struct ospf6_lsa * lsa)
{
  struct ospf6_lsa *old;
  struct ospf6_lsdb *lsdb_self;

  /* find previous LSA */
  old = ospf6_lsdb_lookup (lsa->header->type, lsa->header->id,
                           lsa->header->adv_router, lsa->lsdb);

  /* if the new LSA does not differ from previous,
     suppress this update of the LSA */
  if (old && ! OSPF6_LSA_IS_DIFFER (lsa, old))
  {    
//    if (IS_OSPF6_DEBUG_ORIGINATE_TYPE (lsa->header->type))
//      zlog_debug ("Suppress updating LSA: %s", lsa->name);
    ospf6_lsa_delete (lsa);
    return;
  }  

  /* store it in the LSDB for self-originated LSAs */
  lsdb_self = ospf6_get_scoped_lsdb_self (lsa);
  ospf6_lsdb_add (ospf6_lsa_copy (lsa), lsdb_self);

  lsa->refresh = thread_add_timer (master, ospf6_lsa_refresh, lsa,
                                               LS_REFRESH_TIME);

//  if (IS_OSPF6_DEBUG_LSA_TYPE (lsa->header->type) ||
//      IS_OSPF6_DEBUG_ORIGINATE_TYPE (lsa->header->type))
//  {
//  TODO: encapsulate in DEBUG params
    zlog_debug ("LSA Originate:");
    ospf6_lsa_header_print (lsa);
//  }

//  if (old)
//    ospf6_flood_clear (old);
//  ospf6_flood (NULL, lsa);
  ospf6_install_lsa (lsa);
}

void
ospf6_lsa_originate_interface (struct ospf6_lsa *lsa,
                                   struct ospf6_interface *oi) 
{
    lsa->lsdb = oi->lsdb;
    ospf6_lsa_originate (lsa);
}

/* RFC2328 section 13 The Flooding Procedure */
void ospf6_receive_lsa(struct ospf6_neighbor * from, struct ospf6_lsa_header * lsa_header)
{

}

/* RFC2328 section 13.2 Installing LSAs in the database */
void
ospf6_install_lsa (struct ospf6_lsa *lsa)
{
  struct ospf6_lsa *old;
  struct timeval now;

//  if (IS_OSPF6_DEBUG_LSA_TYPE (lsa->header->type) ||
//      IS_OSPF6_DEBUG_EXAMIN_TYPE (lsa->header->type))
//    zlog_debug ("Install LSA: %s", lsa->name);

  /* Remove the old instance from all neighbors' Link state
     retransmission list (RFC2328 13.2 last paragraph) */
  old = ospf6_lsdb_lookup (lsa->header->type, lsa->header->id,
                           lsa->header->adv_router, lsa->lsdb);

  if (old)
  {
    THREAD_OFF (old->expire);
//    ospf6_flood_clear (old);
  }

  zebralite_gettime (ZEBRALITE_CLK_MONOTONIC, &now);
  if (! OSPF6_LSA_IS_MAXAGE (lsa))
    lsa->expire = thread_add_timer (master, ospf6_lsa_expire, lsa,
                                    MAXAGE + lsa->birth.tv_sec - now.tv_sec);
  else
    lsa->expire = NULL;

  /* actually install */
  lsa->installed = now;
  ospf6_lsdb_add (lsa, lsa->lsdb);

  return;
}

void 
ospf6_lsa_purge(struct ospf6_lsa * lsa)
{
  struct ospf6_lsa * self;
  struct ospf6_lsdb * lsdb_self;

  /* remove it from the LSDB for self-originated LSAs */
  lsdb_self = ospf6_get_scoped_lsdb_self(lsa);
  self = ospf6_lsdb_lookup (lsa->header->type, lsa->header->id,
                            lsa->header->adv_router, lsdb_self);
  if(self)
  {
    THREAD_OFF (self->expire);
    THREAD_OFF (self->refresh);
    ospf6_lsdb_remove (self, lsdb_self);
  }

  ospf6_lsa_premature_aging (lsa);
}
