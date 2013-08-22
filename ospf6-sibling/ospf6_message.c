#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/socket.h>

#include "util.h"
#include "routeflow-common.h"
#include "dblist.h"
#include "debug.h"
#include "thread.h"
#include "rfpbuf.h"
#include "rfp-msgs.h"
#include "if.h"
#include "ctrl_client.h"
#include "ospf6_proto.h"
#include "ospf6_interface.h"
#include "ospf6_message.h"
#include "ospf6_neighbor.h"

extern struct thread_master * master;

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
    zlog_notice("About to send hello message");
  }

  /* set next thread */
  oi->thread_send_hello = thread_add_timer(master, ospf6_hello_send, oi, oi->hello_interval);

  oi->ctrl_client->obuf = routeflow_alloc(RFPT_FORWARD_OSPF6, RFP10_VERSION, sizeof(struct rfp_header));


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

  /* fill OSPF header */
  oh->version = OSPFV3_VERSION;

  // router id
  // area id
  // instance id
  // reserved
  // *NOTE* filled in with zeroes for testing purposes only
  oh->router_id = htonl(0);
  oh->area_id = htonl(0);
  oh->checksum = htons(0);
  oh->instance_id = htons(0);
  oh->reserved = htons(0);

  rfpmsg_update_length(oi->ctrl_client->obuf);
  retval = fwd_message_send(oi->ctrl_client);
  return 0;
}

static void ospf6_hello_recv(struct ospf6_header * oh, struct ospf6_interface * oi)
{
  struct ospf6_hello * hello;
  struct ospf6_neighbor * on;

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

  /* Execute neighbor events */
  thread_execute(master, hello_received, on, 0);
}

static void ospf6_dbdesc_recv(struct ospf6_header * oh, struct ospf6_interface * oi)
{
  struct ospf6_neighbor * on;
  struct ospf6_dbdesc * dbdesc;


  printf("Received DBDESC message\n");

}
int ospf6_receive(struct rfp_header * rh, struct ospf6_interface * oi)
{
  struct ospf6_header * oh;

  oh = (struct ospf6_header *)((void *)rh + sizeof(struct rfp_header));

  switch(oh->type)
  {
    case OSPF6_MESSAGE_TYPE_HELLO:
      ospf6_hello_recv(oh, oi);
      break; 

    case OSPF6_MESSAGE_TYPE_DBDESC:
      ospf6_dbdesc_recv(oh, oi);
      break;

    debault:
      break;
  }
}
