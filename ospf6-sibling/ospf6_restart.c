#include "config.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <pthread.h>

#include <riack.h>

#include "dblist.h"
#include "rfpbuf.h"
#include "routeflow-common.h"
#include "prefix.h"
#include "if.h"
#include "debug.h"
#include "ospf6_interface.h"
#include "ospf6_route.h"
#include "sibling_ctrl.h"
#include "ospf6_top.h"
#include "ospf6_db.h"
#include "ospf6_interface.h"
#include "db.h"
#include "ospf6_restart.h"

bool measurement_started = false;
bool measurement_stopped = false;

static void * ospf6_catchup_start(void * arg);
static void * ospf6_restart_start(void * arg);

void ospf6_restart_init(struct ospf6_interface * oi, bool catchup)
{
  pthread_t * thread = calloc(1, sizeof(pthread_t));

  pthread_mutex_init(&first_xid_mutex, NULL);
  pthread_mutex_lock(&first_xid_mutex);

  pthread_mutex_init(&restart_mode_mutex, NULL);

  pthread_mutex_init(&restart_msg_q_mutex, NULL);

  if(catchup)
    pthread_create(thread, NULL, ospf6_catchup_start, oi);
  else
    pthread_create(thread, NULL, ospf6_restart_start, oi);
}

void process_restarted_msg(unsigned int bid, unsigned int xid, struct ospf6_interface * oi)
{
  struct rfpbuf * msg;
  size_t oh_size;
  struct ospf6_header * oh;
  char * json_buffer;

  oh_size = sizeof(struct ospf6_header);

  msg = rfpbuf_new(oh_size);

  // get the header
  oh = rfpbuf_tail(msg);

  if((json_buffer = ospf6_db_get(oh, xid, bid)) == NULL)
  {
    // we reached end of messages in db
//    rfpbuf_delete(msg);
//    return;
  }
  else
  {
    if(OSPF6_SIBLING_DEBUG_RESTART)
    {
      zlog_debug("Return from ospf6_db_get");
    }    
  }

  msg->size += oh_size;

  // now that we know how big the message is, we can fill in the data too
  rfpbuf_prealloc_tailroom(msg, oh->length - oh_size);

  oh = rfpbuf_at_assert(msg, 0, oh_size);

  ospf6_db_fill_body(json_buffer, oh);

  if(OSPF6_SIBLING_DEBUG_RESTART)
  {
    zlog_debug("Before ospf6_receive");
  }

  ospf6_receive(oi->ctrl_client, oh, xid, oi);

  rfpbuf_delete(msg);
}

void * ospf6_catchup_start(void * arg)
{
  unsigned int replica_id;
  unsigned int leader_id;
  unsigned int xid;
  unsigned int leader_xid;
  unsigned int last_key_to_process;
  unsigned int first_xid_rcvd;

  struct rfpbuf * own_msg;
  struct list * restart_msg_queue;
  struct rfp_header * rh;
  struct ospf6_header * oh;
  struct interface * ifp;
  struct ospf6_interface * oi;

  struct keys * keys;

  int i;

  oi = (struct ospf6_interface *)arg;

  if(OSPF6_SIBLING_DEBUG_RESTART)
  {
    zlog_debug("In ospf6_catchup_start");
  }

  replica_id = ospf6_replica_get_id();

  last_key_to_process = ospf6_db_last_key(replica_id);

  if(OSPF6_SIBLING_DEBUG_RESTART)
  {
    zlog_debug("Last key to process: %d", last_key_to_process);
  }


  if(!ospf6->ready_to_checkpoint)
  {
    ospf6->ready_to_checkpoint = true;
  }

  if(OSPF6_SIBLING_DEBUG_RESTART)
  {
    zlog_debug("We are ready to checkpoint");
  }

  // try to acquire the lock, block if not ready yet
  pthread_mutex_lock(&first_xid_mutex);

  if(OSPF6_SIBLING_DEBUG_RESTART)
  {
    zlog_debug("Before mutex_lock");
  }
  
  first_xid_rcvd = sibling_ctrl_first_xid_rcvd();

  if(OSPF6_SIBLING_DEBUG_RESTART)
  {
    zlog_debug("After mutex_lock");
  }

  pthread_mutex_unlock(&first_xid_mutex);

  leader_id = ospf6_replica_get_leader_id();

  leader_xid = ospf6_db_get_current_xid(leader_id);
 
  if(OSPF6_SIBLING_DEBUG_RESTART)
  {
    zlog_debug("Leader xid: %d", leader_xid);
  }

 
  // get all the keys from next after last processed key until the first received xid
  // this may not work, will need to test it
  keys = ospf6_db_range_keys(leader_id, last_key_to_process+1, leader_xid);

  for(i = 0; i < keys->num_keys; i++)
  {
    if(OSPF6_SIBLING_DEBUG_RESTART) {
      zlog_debug("key to process that is coming from the leader: %s", keys->key_str_ptrs[i]);
    }

    xid = (unsigned int) strtol(keys->key_str_ptrs[i], NULL, 10);
    process_restarted_msg(leader_id, xid, oi);
  }


 // no longer need the keys structure, free it again
  db_free_keys(keys);

  restart_msg_queue = sibling_ctrl_restart_msg_queue();

  pthread_mutex_lock(&restart_msg_q_mutex);
  LIST_FOR_EACH(own_msg, struct rfpbuf, list_node, restart_msg_queue)
  {
    rh = rfpbuf_at_assert(own_msg, 0, sizeof(struct rfp_header));
    oh = (struct ospf6_header *)((void *)rh + sizeof(struct rfp_header));
    xid = ntohl(rh->xid);

    if(!(xid < leader_xid))
    {
      ospf6_receive(oi->ctrl_client, oh, xid, oi);
    }
  }
  pthread_mutex_unlock(&restart_msg_q_mutex);

  if(OSPF6_SIBLING_DEBUG_RESTART)
  {
    zlog_debug("We are done processing in restart mode");
  }

  // flip the switch, we are no longer in restart mode
  pthread_mutex_lock(&restart_mode_mutex);
  if(ospf6->restart_mode)
    ospf6->restart_mode = false; 
  pthread_mutex_unlock(&restart_mode_mutex);

  return NULL;

}

void * ospf6_restart_start(void * arg)
{
  unsigned int replica_id;
  unsigned int leader_id;
  unsigned int xid;
  unsigned int last_key_to_process;
  unsigned int first_xid_rcvd;

  struct rfpbuf * own_msg;
  struct list * restart_msg_queue;
  size_t restart_msg_queue_num_msgs;

  struct rfp_header * rh;
  struct ospf6_header * oh;
  struct interface * ifp;
  struct ospf6_interface * oi;

  long int timestamp_sec;
  long int timestamp_msec;

  struct keys * keys;
  
  struct timespec time;

  int leader_xid;

  oi = (struct ospf6_interface *)arg;

  if(OSPF6_SIBLING_DEBUG_RESTART)
  {
    zlog_debug("In ospf6_restart_start");

    if(!measurement_started)
    {
      measurement_started = true;
      clock_gettime(CLOCK_REALTIME, &time);
      zlog_debug("[%ld.%09ld]: Started measurement", time.tv_sec, time.tv_nsec);
    }
  }

  replica_id = ospf6_replica_get_id();


  keys = ospf6_db_list_keys(replica_id);

  int i;

  for(i = 0; i < keys->num_keys; i++)
  {
    if(OSPF6_SIBLING_DEBUG_RESTART)
    {
      zlog_debug("key to process that is coming from the replica: %s", keys->key_str_ptrs[i]);
    }

    xid = (unsigned int) strtol(keys->key_str_ptrs[i], NULL, 10);

    process_restarted_msg(replica_id, xid, oi);

    //last_timestamp_to_process = (long int)strtol(keys->key_str_ptrs[i], NULL, 10);
    last_key_to_process = xid;
  }

  if(!measurement_stopped)
  {
    measurement_stopped = true;
    clock_gettime(CLOCK_REALTIME, &time);
    zlog_debug("[%ld.%09ld]: Stopped measurement", time.tv_sec, time.tv_nsec);
  }

// no longer need the keys structure, free it here
  db_free_keys(keys);

  if(!ospf6->ready_to_checkpoint)
  {
    ospf6->ready_to_checkpoint = true;
  }

  if(OSPF6_SIBLING_DEBUG_RESTART)
  {
    zlog_debug("We are ready to checkpoint");
  }

  // only needed when not attached to external ospf6
//  if((restart_msg_queue_num_msgs = sibling_ctrl_restart_msg_queue_num_msgs()) == 0)
//  {
//    pthread_mutex_unlock(&first_xid_mutex);
//  }

  // try to acquire the lock, block if not ready yet
  pthread_mutex_lock(&first_xid_mutex);

  if(OSPF6_SIBLING_DEBUG_RESTART)
  {
    zlog_debug("Before mutex_lock");
  }
 
 // only needed when not attached to external ospf6 
  if(restart_msg_queue_num_msgs != 0)
  {
    first_xid_rcvd = sibling_ctrl_first_xid_rcvd();
  }

  if(OSPF6_SIBLING_DEBUG_RESTART)
  {
    zlog_debug("After mutex_lock");
  }

  pthread_mutex_unlock(&first_xid_mutex);

  clock_gettime(CLOCK_REALTIME, &time);
  zlog_debug("[%ld.%09ld]: First message received", time.tv_sec, time.tv_nsec);


  leader_id = ospf6_replica_get_leader_id();

  if(OSPF6_SIBLING_DEBUG_RESTART)
  {
    zlog_debug("Before calling db_range_keys");
  }

  // only needed when not attached to external ospf6 
  if(restart_msg_queue_num_msgs != 0)
  {
    // get all the keys from next after last processed key until the first received xid
    // this may not work, will need to test it
    keys = ospf6_db_range_keys(leader_id, last_key_to_process+1, first_xid_rcvd-1);
  }

  if(OSPF6_SIBLING_DEBUG_RESTART)
  {
    zlog_debug("After calling db_range_keys");
  }


  // only needed when not attached to external ospf6 
  if(restart_msg_queue_num_msgs != 0)
  {
    // i is an index here into the keys, not the actual xid
    for(i = 0; i < keys->num_keys; i++)
    {
      if(OSPF6_SIBLING_DEBUG_RESTART)
      {
        zlog_debug("key to process that is coming from the leader: %s", keys->key_str_ptrs[i]);
      }

      xid = (unsigned int) strtol(keys->key_str_ptrs[i], NULL, 10);
      process_restarted_msg(leader_id, xid, oi);
    }
  }

  clock_gettime(CLOCK_REALTIME, &time);
  zlog_debug("[%ld.%09ld]: Keys from leader's replica", time.tv_sec, time.tv_nsec);

  // only needed when not attached to external ospf6 
  if(restart_msg_queue_num_msgs != 0)
  {
    // no longer need the keys structure, free it again
    db_free_keys(keys);
  }

  // only needed when not attached to external ospf6 
  if(restart_msg_queue_num_msgs != 0)
  {
    restart_msg_queue = sibling_ctrl_restart_msg_queue();
  }

  // only needed when not attached to external ospf6 
  if(restart_msg_queue_num_msgs != 0)
  {
  
    pthread_mutex_lock(&restart_msg_q_mutex);
    LIST_FOR_EACH(own_msg, struct rfpbuf, list_node, restart_msg_queue)
    {
      rh = rfpbuf_at_assert(own_msg, 0, sizeof(struct rfp_header));
      oh = (struct ospf6_header *)((void *)rh + sizeof(struct rfp_header));
      xid = ntohl(rh->xid);

      ospf6_receive(oi->ctrl_client, oh, xid, oi);
    }
    pthread_mutex_unlock(&restart_msg_q_mutex);

  }

  if(OSPF6_SIBLING_DEBUG_RESTART)
  {
    zlog_debug("We are done processing in restart mode");
 
//    if(!measurement_stopped)
//    {
//      measurement_stopped = true;
//      clock_gettime(CLOCK_REALTIME, &time);
//      zlog_debug("[%ld.%09ld]: Stopped measurement", time.tv_sec, time.tv_nsec);
//    }
 }

  // flip the switch, we are no longer in restart mode
  pthread_mutex_lock(&restart_mode_mutex);
  if(ospf6->restart_mode)
    ospf6->restart_mode = false; 
  pthread_mutex_unlock(&restart_mode_mutex);

  if(OSPF6_SIBLING_DEBUG_RESTART)
  {
    zlog_debug("We are no longer in restart mode");
  }

  return NULL;
}
