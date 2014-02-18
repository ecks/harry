#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <pthread.h>

#include "dblist.h"
#include "rfpbuf.h"
#include "routeflow-common.h"
#include "if.h"
#include "debug.h"
#include "sibling_ctrl.h"
#include "ospf6_interface.h"
#include "ospf6_db.h"
#include "ospf6_restart.h"

static void * ospf6_restart_start(void * arg);

void ospf6_restart_init(char * interface)
{
  pthread_t * thread = calloc(1, sizeof(pthread_t));

  pthread_mutex_init(&first_xid_mutex, NULL);
  pthread_mutex_lock(&first_xid_mutex);

  pthread_create(thread, NULL, ospf6_restart_start, interface);
}

void * ospf6_restart_start(void * arg)
{
  unsigned int replica_id;
  unsigned int xid;
  unsigned int first_xid_rcvd;
  size_t oh_size;
  struct ospf6_header * oh;

  struct keys * keys;
  
  struct rfpbuf * msg;
  char * json_buffer;

  char * interface_name = (char *)arg;
  struct interface * ifp;
  struct ospf6_interface * oi;

  oh_size = sizeof(struct ospf6_header);

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
      zlog_debug("key: %s", keys->key_str_ptrs[i]);
    }

    xid = (unsigned int) strtol(keys->key_str_ptrs[i], NULL, 10);

    msg = rfpbuf_new(oh_size);

    // get the header
    oh = rfpbuf_tail(msg);

    if((json_buffer = ospf6_db_get(oh, xid, replica_id)) == NULL)
    {
      // we reached end of messages in db
      rfpbuf_delete(msg);
      break;
    }

    msg->size += oh_size;

    // now that we know how big the message is, we can fill in the data too
    rfpbuf_prealloc_tailroom(msg, oh->length - oh_size);

    oh = rfpbuf_at_assert(msg, 0, oh_size);

    ospf6_db_fill_body(json_buffer, oh);

    if(OSPF6_SIBLING_DEBUG_RESTART)
    {
      zlog_debug("In ospf6_restart_start4, if_name: %s", interface_name);
    }

    ifp = if_get_by_name(interface_name);
    oi = (struct ospf6_interface *)ifp->info;

    if(OSPF6_SIBLING_DEBUG_RESTART)
    {
      zlog_debug("In ospf6_restart_start5");
    }

    ospf6_receive(NULL, oh, xid, oi);

    rfpbuf_delete(msg);
  }

  // try to acquire the lock, block if not ready yet
  pthread_mutex_lock(&first_xid_mutex);

  first_xid_rcvd = sibling_ctrl_first_xid_rcvd();

  if(OSPF6_SIBLING_DEBUG_RESTART)
  {
    zlog_debug("first_xid_rcvd: %d", first_xid_rcvd);
  }

  return NULL;
}
