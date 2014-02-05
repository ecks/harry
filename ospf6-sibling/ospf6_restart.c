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
#include "ospf6_interface.h"
#include "ospf6_db.h"
#include "ospf6_restart.h"

static void * ospf6_restart_start(void * arg);

void ospf6_restart_init(char * interface)
{
  pthread_t * thread = calloc(1, sizeof(pthread_t));

  pthread_create(thread, NULL, ospf6_restart_start, interface);
}

void * ospf6_restart_start(void * arg)
{
  unsigned int replica_id;
  unsigned int xid;
  size_t oh_size;
  struct ospf6_header * oh;

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

  msg = rfpbuf_new(oh_size);

  replica_id = ospf6_replica_get_id();
  xid = 0;

  // get the header
  oh = rfpbuf_tail(msg);

  json_buffer = ospf6_db_get(oh, xid, replica_id);
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

  return NULL;
}
