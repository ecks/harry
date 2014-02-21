#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <pthread.h>

#include <riack.h>

#include "dblist.h"
#include "rfpbuf.h"
#include "routeflow-common.h"
#include "if.h"
#include "debug.h"
#include "sibling_ctrl.h"
#include "ospf6_top.h"
#include "ospf6_db.h"
#include "ospf6_interface.h"
#include "db.h"
#include "ospf6_restart.h"

static void * ospf6_restart_start(void * arg);

void ospf6_restart_init(char * interface)
{
  pthread_t * thread = calloc(1, sizeof(pthread_t));

  pthread_mutex_init(&first_xid_mutex, NULL);
  pthread_mutex_lock(&first_xid_mutex);

  pthread_mutex_init(&restart_mode_mutex, NULL);

  pthread_mutex_init(&restart_msg_q_mutex, NULL);

  pthread_create(thread, NULL, ospf6_restart_start, interface);
}

void process_restarted_msg(unsigned int bid, unsigned int xid, char * interface_name)
{
  struct rfpbuf * msg;
  size_t oh_size;
  struct ospf6_header * oh;
  char * json_buffer;
  struct interface * ifp;
  struct ospf6_interface * oi;

  oh_size = sizeof(struct ospf6_header);

  msg = rfpbuf_new(oh_size);

  // get the header
  oh = rfpbuf_tail(msg);

  if((json_buffer = ospf6_db_get(oh, xid, bid)) == NULL)
  {
    // we reached end of messages in db
    rfpbuf_delete(msg);
    return;
  }

  msg->size += oh_size;

  // now that we know how big the message is, we can fill in the data too
  rfpbuf_prealloc_tailroom(msg, oh->length - oh_size);

  oh = rfpbuf_at_assert(msg, 0, oh_size);

  ospf6_db_fill_body(json_buffer, oh);

  ifp = if_get_by_name(interface_name);
  oi = (struct ospf6_interface *)ifp->info;

  ospf6_receive(NULL, oh, xid, oi);

  rfpbuf_delete(msg);
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
  struct rfp_header * rh;
  struct ospf6_header * oh;
  struct interface * ifp;
  struct ospf6_interface * oi;

  struct keys * keys;
  
  char * interface_name = (char *)arg;

  if(OSPF6_SIBLING_DEBUG_RESTART)
  {
    zlog_debug("In ospf6_restart_start");
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

    process_restarted_msg(replica_id, xid, interface_name);

    last_key_to_process = (unsigned int)strtol(keys->key_str_ptrs[i], NULL, 10);
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

  // try to acquire the lock, block if not ready yet
  pthread_mutex_lock(&first_xid_mutex);
  first_xid_rcvd = sibling_ctrl_first_xid_rcvd();
  pthread_mutex_unlock(&first_xid_mutex);

  leader_id = ospf6_replica_get_leader_id();

  // get all the keys from next after last processed key until the first received xid
  keys = ospf6_db_range_keys(leader_id, last_key_to_process+1, first_xid_rcvd);

  for(i = 0; i < keys->num_keys; i++)
  {
    if(OSPF6_SIBLING_DEBUG_RESTART)
    {
      zlog_debug("key to process that is coming from the leader: %s", keys->key_str_ptrs[i]);
    }

    xid = (unsigned int) strtol(keys->key_str_ptrs[i], NULL, 10);

    process_restarted_msg(leader_id, xid, interface_name);
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
    ifp = if_get_by_name(interface_name);
    oi = (struct ospf6_interface *)ifp->info;

    ospf6_receive(NULL, oh, xid, oi);
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
