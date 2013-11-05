#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <assert.h>

#include "util.h"
#include "routeflow-common.h"
#include "dblist.h"
#include "debug.h"
#include "thread.h"
#include "rfpbuf.h"
#include "rfp-msgs.h"
#include "if.h"
#include "ctrl_client.h"
#include "ospf6_top.h"
#include "ospf6_proto.h"
#include "ospf6_lsa.h"
#include "ospf6_lsdb.h"
#include "ospf6_interface.h"
#include "ospf6_area.h"
#include "ospf6_message.h"
#include "ospf6_neighbor.h"

/* global ospf6d variable */
struct ospf6 *ospf6;

extern struct thread_master * master;

static void ospf6_fill_header(struct ospf6_interface * oi, struct ospf6_header * oh)
{
  /* fill OSPF header */
  oh->version = OSPFV3_VERSION;

  /* message type must be set before */
  /* message length must be set before */
  oh->router_id = oi->area->ospf6->router_id;
  oh->area_id = oi->area->area_id;
  // *NOTE* filled in with zeroes for testing purposes only
  oh->instance_id = htons(0);
  oh->reserved = htons(0);


}

int ospf6_hello_send(struct thread * thread)
{
  u_char * p;
  struct ospf6_interface * oi;
  int retval;

  oi = (struct ospf6_interface *)THREAD_ARG(thread);
  struct rfp_header * rh;
  struct ospf6_header * oh;
  struct ospf6_hello * hello;

  oi->thread_send_hello = (struct thread *)NULL;

  if(oi->interface->state == RFPPS_LINK_DOWN)
  {
    printf("Tried to send hello message on link that is down\n");
    return 0;
  }

  if(IS_OSPF6_SIBLING_DEBUG_MSG)
  {
    zlog_debug("About to send hello message");
  }

  /* set next thread */
  oi->thread_send_hello = thread_add_timer(master, ospf6_hello_send, oi, oi->hello_interval);

  oi->ctrl_client->obuf = routeflow_alloc_xid(RFPT_FORWARD_OSPF6, RFP10_VERSION, 
                                              htonl(oi->ctrl_client->current_xid), sizeof(struct rfp_header));


  oh = rfpbuf_put_uninit(oi->ctrl_client->obuf, sizeof(struct ospf6_header));

  hello = rfpbuf_put_uninit(oi->ctrl_client->obuf, sizeof(struct ospf6_hello));

  hello->interface_id = htonl(oi->interface->ifindex);
  hello->priority = oi->priority;
//  hello->options[0] = oi->area->options[0];
//  hello->options[1] = oi->area->options[1];
//  hello->options[2] = oi->area->options[2];
  // for now set the options directly since we don't have an area yet
  OSPF6_OPT_SET(hello->options, OSPF6_OPT_V6);
  OSPF6_OPT_SET(hello->options, OSPF6_OPT_E);
  OSPF6_OPT_SET(hello->options, OSPF6_OPT_R);

  hello->hello_interval = htons (oi->hello_interval);
  hello->dead_interval = htons (oi->dead_interval);
//  hello->drouter = oi->drouter;
//  hello->bdrouter = oi->bdrouter;
//*NOTE* this is only for testing, needs to be removed soon because it is incorrect
  hello->drouter = htonl(0);
  hello->bdrouter = htonl(0);

  p = (u_char *)((void *)hello + sizeof(struct ospf6_hello));

  struct ospf6_neighbor * on;
  int i = 0;

  LIST_FOR_EACH(on, struct ospf6_neighbor, node, &oi->neighbor_list)
  {
    if(on->state < OSPF6_NEIGHBOR_INIT)
      continue;

    if((void *)p - oi->ctrl_client->obuf->data + sizeof(u_int32_t) > oi->interface->mtu)
    {
      printf("mtu exceeded\n");
      break;
    }

    // preallocate more space in the buffer if necessary
    rfpbuf_prealloc_tailroom(oi->ctrl_client->obuf, sizeof(u_int32_t));

    // need to reset all references in case they have changed as a result of realloc
    rh = oi->ctrl_client->obuf->l2;
    oh = (struct ospf6_header *)((void *)rh + sizeof(struct rfp_header));
    hello = (struct ospf6_hello *)((void *)oh + sizeof(struct ospf6_header));
    p = (u_char *)((void *)hello + sizeof(struct ospf6_hello) + i*sizeof(u_int32_t));

    memcpy(p, &on->router_id, sizeof(u_int32_t));
    oi->ctrl_client->obuf->size += sizeof(u_int32_t);

    p += sizeof(u_int32_t);

    i++;
  }

  // do this at the end
  oh->type = OSPF6_MESSAGE_TYPE_HELLO;
  oh->length = htons(p - (u_char *)(oh));

  ospf6_fill_header(oi, oh);

  rfpmsg_update_length(oi->ctrl_client->obuf);
  retval = fwd_message_send(oi->ctrl_client);

  // increment the xid after we send the message
  oi->ctrl_client->current_xid++;
  return 0;
}

int ospf6_dbdesc_send(struct thread * thread)
{
  struct ospf6_neighbor * on;
  struct ospf6_header * oh;
  struct ospf6_dbdesc * dbdesc;
  int retval;
  u_char * p;
  struct ospf6_lsa * lsa;

  on = (struct ospf6_neighbor *)THREAD_ARG(thread);
  on->thread_send_dbdesc = (struct thread *) NULL;

  if (on->state < OSPF6_NEIGHBOR_EXSTART)
  {
    return 0;
  }

  if(IS_OSPF6_SIBLING_DEBUG_MSG)
  {
    zlog_debug("About to send dbdesc message");
  }

  /* set next thread if master */
  if (CHECK_FLAG (on->dbdesc_bits, OSPF6_DBDESC_MSBIT))
    on->thread_send_dbdesc =
      thread_add_timer (master, ospf6_dbdesc_send, on,
                        on->ospf6_if->rxmt_interval);

  on->ospf6_if->ctrl_client->obuf = routeflow_alloc_xid(RFPT_FORWARD_OSPF6, RFP10_VERSION,
                                                        htonl(on->ospf6_if->ctrl_client->current_xid), sizeof(struct rfp_header));

  oh = rfpbuf_put_uninit(on->ospf6_if->ctrl_client->obuf, sizeof(struct ospf6_header));
  dbdesc = rfpbuf_put_uninit(on->ospf6_if->ctrl_client->obuf, sizeof(struct ospf6_dbdesc));

  /* if this is initial one, initialize sequence number for DbDesc */
  if (CHECK_FLAG (on->dbdesc_bits, OSPF6_DBDESC_IBIT))
  {    
    struct timeval tv;
    if (zebralite_gettime (ZEBRALITE_CLK_MONOTONIC, &tv) < 0) 
                                tv.tv_sec = 1; 
                            on->dbdesc_seqnum = tv.tv_sec;
  }    

  dbdesc->options[0] = on->ospf6_if->area->options[0];
  dbdesc->options[1] = on->ospf6_if->area->options[1];
  dbdesc->options[2] = on->ospf6_if->area->options[2];

  dbdesc->ifmtu = htons (on->ospf6_if->ifmtu);
  dbdesc->bits = on->dbdesc_bits;
  dbdesc->seqnum = htonl (on->dbdesc_seqnum);

  /* if this is not initial one, set LSA headers in dbdesc */
  p = (u_char *)((void *) dbdesc + sizeof (struct ospf6_dbdesc));
  if (! CHECK_FLAG (on->dbdesc_bits, OSPF6_DBDESC_IBIT))
  {
    for (lsa = ospf6_lsdb_head (on->dbdesc_list); lsa; 
         lsa = ospf6_lsdb_next (lsa))
    {    
      ospf6_lsa_age_update_to_send (lsa, on->ospf6_if->transdelay);

      /* MTU check */
      if (p - (u_char *)oh + sizeof (struct ospf6_lsa_header) >
          on->ospf6_if->ifmtu)
      {
        ospf6_lsa_unlock (lsa);
        break;
      }
      memcpy (p, lsa->header, sizeof (struct ospf6_lsa_header));
      p += sizeof (struct ospf6_lsa_header);
    }
  }

  oh->type = OSPF6_MESSAGE_TYPE_DBDESC;
  oh->length = htons(p - (u_char *)(oh));

  ospf6_fill_header(on->ospf6_if, oh);

  rfpmsg_update_length(on->ospf6_if->ctrl_client->obuf);
  retval = fwd_message_send(on->ospf6_if->ctrl_client);

  // increment the xid after we send the message
  on->ospf6_if->ctrl_client->current_xid++;
  return 0;
}

int ospf6_dbdesc_send_newone(struct thread * thread)
{
  struct ospf6_neighbor *on; 
  struct ospf6_lsa *lsa;
  unsigned int size = 0; 

  on = (struct ospf6_neighbor *) THREAD_ARG (thread);
  ospf6_lsdb_remove_all (on->dbdesc_list);

  /* move LSAs from summary_list to dbdesc_list (within neighbor structure)
    so that ospf6_send_dbdesc () can send those LSAs */
  size = sizeof (struct ospf6_lsa_header) + sizeof (struct ospf6_dbdesc);
  for (lsa = ospf6_lsdb_head (on->summary_list); lsa; 
       lsa = ospf6_lsdb_next (lsa))
  {    
    if (size + sizeof (struct ospf6_lsa_header) > on->ospf6_if->ifmtu)
    {    
      ospf6_lsa_unlock (lsa);
      break;
    }    

    ospf6_lsdb_add (ospf6_lsa_copy (lsa), on->dbdesc_list);
    ospf6_lsdb_remove (lsa, on->summary_list);
    size += sizeof (struct ospf6_lsa_header);
  }    

  if (on->summary_list->count == 0)
    UNSET_FLAG (on->dbdesc_bits, OSPF6_DBDESC_MBIT);

  /* If slave, More bit check must be done here */
  if (! CHECK_FLAG (on->dbdesc_bits, OSPF6_DBDESC_MSBIT) && /* Slave */
      ! CHECK_FLAG (on->dbdesc_last.bits, OSPF6_DBDESC_MBIT) &&
      ! CHECK_FLAG (on->dbdesc_bits, OSPF6_DBDESC_MBIT))
    thread_add_event (master, exchange_done, on, 0);

  thread_execute (master, ospf6_dbdesc_send, on, 0);
  return 0;
}

static void ospf6_hello_recv(struct ctrl_client * ctrl_client, struct ospf6_header * oh, 
                             struct ospf6_interface * oi, unsigned int xid)
{
  struct ospf6_hello * hello;
  struct ospf6_neighbor * on;
  char * p;
  int twoway = 0;
  int retval;

  if(IS_OSPF6_SIBLING_DEBUG_MSG)
  {
    zlog_debug("ospf6 hello received");
  }

  hello = (struct ospf6_hello *)((void *)oh + sizeof(struct ospf6_header));

  /* HelloInterval check */
  if(ntohs(hello->hello_interval) != oi->hello_interval)
  {
    printf("HelloInterval mismatch\n");
    return;
  }

  /* RouterDeadInterval */

  /* E-bit check */

  /* Find neighbor, create if not exist */
  on = ospf6_neighbor_lookup(oh->router_id, oi);
  if(on == NULL)
  {
    on = ospf6_neighbor_create(oh->router_id, oi);
    on->prev_drouter = on->drouter = hello->drouter;
    on->prev_bdrouter = on->bdrouter = hello->bdrouter;
    on->priority = hello->priority;
  }

  /* always override neighbor's source address and ifindex */
  on->ifindex = ntohl(hello->interface_id);
//  memcpy(&on->linklocal_addr, src, sizeof(struct in6_addr));

  /* TwoWay check */
  for(p = (char *)((void *)hello + sizeof(struct ospf6_hello));
      p + sizeof(u_int32_t) <= OSPF6_MESSAGE_END(oh);
      p += sizeof(u_int32_t))
  {
    u_int32_t * router_id = (u_int32_t *)p;

    if(*router_id  == ospf6->router_id)  // should really be oi->area->ospf6->router_id
      twoway++;
  }

  /* Execute neighbor events */
  thread_execute(master, hello_received, on, 0);
  if(twoway)
    thread_execute(master, twoway_received, on, 0);
  else
    thread_execute(master, oneway_received, on, 0);

  // send an ACK back to the controller
  ctrl_client->obuf = routeflow_alloc_xid(RFPT_ACK, RFP10_VERSION, 
                                          htonl(xid), sizeof(struct rfp_header));
  retval = ctrl_send_message(ctrl_client);
}

static void ospf6_dbdesc_recv_master(struct ospf6_header * oh,
                                     struct ospf6_neighbor * on)
{
  if(IS_OSPF6_SIBLING_DEBUG_MSG)
  {
    zlog_debug("router is master");
  }
}

static void ospf6_dbdesc_recv_slave(struct ospf6_header * oh,
                                    struct ospf6_neighbor * on)
{
  struct ospf6_dbdesc * dbdesc;
  char * p;

  if(OSPF6_SIBLING_DEBUG_MSG)
  {
    zlog_debug("router is slave");
  }

  dbdesc = (struct ospf6_dbdesc *)((void *)oh + sizeof(struct ospf6_header));

  if(on->state < OSPF6_NEIGHBOR_INIT)
  {
    if(IS_OSPF6_SIBLING_DEBUG_MSG)
      zlog_debug("Neighbor state less than Init, ignore");
      return;
  }

  switch(on->state)
  {
    case OSPF6_NEIGHBOR_TWOWAY:
      if(IS_OSPF6_SIBLING_DEBUG_MSG)
        zlog_debug("Neighbor state is 2-Way, ignore");
      return;

    case OSPF6_NEIGHBOR_INIT:
      thread_execute(master, twoway_received, on, 0);
      if(on->state != OSPF6_NEIGHBOR_EXSTART)
      {
        if(IS_OSPF6_SIBLING_DEBUG_MSG)
          zlog_debug("Neighbor state is not ExStart, ignore");
        return;
      }
      /* else fall through to ExStart */

    case OSPF6_NEIGHBOR_EXSTART:
      /* If the neighbor is Master, act as Slave. Schedule negotiation_done
          and process LSA Headers. Otherwise, ignore this message */
      if (CHECK_FLAG (dbdesc->bits, OSPF6_DBDESC_IBIT) &&
          CHECK_FLAG (dbdesc->bits, OSPF6_DBDESC_MBIT) &&
          CHECK_FLAG (dbdesc->bits, OSPF6_DBDESC_MSBIT) &&
          ntohs (oh->length) == sizeof (struct ospf6_header) +
                                sizeof (struct ospf6_dbdesc))
      {    
        /* set the master/slave bit to slave */
        UNSET_FLAG (on->dbdesc_bits, OSPF6_DBDESC_MSBIT);

        /* set the DD sequence number to one specified by master */
        on->dbdesc_seqnum = ntohl (dbdesc->seqnum);

        /* schedule NegotiationDone */
        thread_execute (master, negotiation_done, on, 0);

        /* Record neighbor options */
        memcpy (on->options, dbdesc->options, sizeof (on->options));
      }    
      else 
      {    
        if (IS_OSPF6_SIBLING_DEBUG_MSG)
          zlog_debug ("Negotiation failed");
          return;
      }    
      break;

    case OSPF6_NEIGHBOR_EXCHANGE:

    case OSPF6_NEIGHBOR_LOADING:
    case OSPF6_NEIGHBOR_FULL:
      break;

    default:
      assert(0);
      break;
  }

  /* Process LSA headers */
  for(p = (char *) ((void *)dbdesc + sizeof(struct ospf6_dbdesc));
      p + sizeof(struct ospf6_lsa_header) <= ((void *) (oh) + ntohs((oh)->length));
      p += sizeof(struct ospf6_lsa_header))
  {
     if(OSPF6_SIBLING_DEBUG_MSG)
    {
      zlog_debug("processing lsa inside dbdesc");
    }
   
    struct ospf6_lsa * his, * mine;
    struct ospf6_lsdb * lsdb = NULL;

    his = ospf6_lsa_create_headeronly((struct ospf6_lsa_header *) p);

    switch(OSPF6_LSA_SCOPE(his->header->type))
    {
      case OSPF6_SCOPE_LINKLOCAL:
        lsdb = on->ospf6_if->lsdb;
        break;
      case OSPF6_SCOPE_AREA:
        // NOT IMPLEMENTED
        break;
      case OSPF6_SCOPE_AS:
        lsdb = ospf6->lsdb;
        break;
      case OSPF6_SCOPE_RESERVED:
        ospf6_lsa_delete(his);
        break;
    }

    // E-bit mismatch

    mine = ospf6_lsdb_lookup(his->header->type, his->header->id,
                             his->header->adv_router, lsdb);
    if(mine == NULL || ospf6_lsa_compare(his, mine) < 0)
    {
      ospf6_lsdb_add(his, on->request_list);
    }
    else
      ospf6_lsa_delete(his);
  }

  if(p != OSPF6_MESSAGE_END(oh))
  {
    if(IS_OSPF6_SIBLING_DEBUG_MSG)
      zlog_debug("Trailing garbage ignored");

  }

  /* Set sequence number to Master's */
  on->dbdesc_seqnum = ntohl(dbdesc->seqnum);

  THREAD_OFF(on->thread_send_dbdesc);
  on->thread_send_dbdesc = thread_add_event(master, ospf6_dbdesc_send_newone, on, 0);

  /* save last received dbdesc */
  memcpy(&on->dbdesc_last, dbdesc, sizeof(struct ospf6_dbdesc));
}

static void ospf6_dbdesc_recv(struct ctrl_client * ctrl_client,
                              struct ospf6_header * oh, 
                              struct ospf6_interface * oi,
                              unsigned int xid)
{
  struct ospf6_neighbor * on;
  struct ospf6_dbdesc * dbdesc;
  int retval;

  if(IS_OSPF6_SIBLING_DEBUG_MSG)
  {
    zlog_debug("Received DBDESC message");
  }

  on = ospf6_neighbor_lookup(oh->router_id, oi);
  if(on == NULL)
  {
    printf("Neighbor not found, ignore");   
    return;
  }

  dbdesc = (struct ospf6_dbdesc *)((void *)oh + sizeof(struct ospf6_header));

  /* Interface MTU check */
  if(ntohs(dbdesc->ifmtu) != oi->ifmtu)
    if(IS_OSPF6_SIBLING_DEBUG_MSG)
    {
      zlog_debug("I/F MTU mismatch");
      return;
    }

  // reserved bits
  
  if(ntohl(oh->router_id) < ntohl(ospf6->router_id))
    ospf6_dbdesc_recv_master(oh, on);
  else if(ntohl(ospf6->router_id) < ntohl(oh->router_id))
    ospf6_dbdesc_recv_slave(oh, on);
  else
  {
    if(IS_OSPF6_SIBLING_DEBUG_MSG)
      zlog_debug("Can't decide which is master, ignore");
  }

  // send an ACK back to the controller
  ctrl_client->obuf = routeflow_alloc_xid(RFPT_ACK, RFP10_VERSION, 
                                          htonl(xid), sizeof(struct rfp_header));
  retval = ctrl_send_message(ctrl_client);
}

int ospf6_receive(struct ctrl_client * ctrl_client, 
                  struct rfp_header * rh, 
                  struct ospf6_interface * oi)
{
  struct ospf6_header * oh;
  unsigned int xid;

  xid = ntohl(rh->xid);

  oh = (struct ospf6_header *)((void *)rh + sizeof(struct rfp_header));

  switch(oh->type)
  {
    case OSPF6_MESSAGE_TYPE_HELLO:
      ospf6_hello_recv(ctrl_client, oh, oi, xid);
      break; 

    case OSPF6_MESSAGE_TYPE_DBDESC:
      ospf6_dbdesc_recv(ctrl_client, oh, oi, xid);
      break;

    debault:
      break;
  }

  return 0;
}
