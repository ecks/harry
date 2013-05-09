#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/socket.h>

#include "routeflow-common.h"
#include "dblist.h"
#include "thread.h"
#include "rfpbuf.h"
#include "rfp-msgs.h"
#include "if.h"
#include "ctrl_client.h"
#include "ospf6_proto.h"
#include "ospf6_interface.h"
#include "ospf6_message.h"

extern struct thread_master * master;

int ospf6_hello_send(struct thread * thread)
{
  struct ospf6_interface * oi;
  int retval;

  oi = (struct ospf6_interface *)THREAD_ARG(thread);
  struct rfp_forward_ospf6 * rfo;
  struct ospf6_header * oh;
  struct ospf6_hello * hello;

  oi->thread_send_hello = (struct thread *)NULL;

  if(oi->interface->state == RFPPS_LINK_DOWN)
  {
    printf("Tried to send hello message on link that is down\n");
    return 0;
  }

  printf("About to send hello message\n");

  /* set next thread */
  oi->thread_send_hello = thread_add_timer(master, ospf6_hello_send, oi, oi->hello_interval);

  oi->ctrl_client->obuf = routeflow_alloc(RFPT_FORWARD_OSPF6, RFP10_VERSION, sizeof(struct rfp_forward_ospf6));

  rfo = oi->ctrl_client->obuf->l2;

  oh = &rfo->ospf6_header;

  hello = &rfo->ospf6_hello;

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

  // this is where you put the router ids of the neighbors

  // do this at the end
  oh->type = OSPF6_MESSAGE_TYPE_HELLO;
  oh->length = htons(sizeof(struct ospf6_header) + sizeof(struct ospf6_hello));

  /* fill OSPF header */
  oh->version = OSPFV3_VERSION;

  // router id
  // area id
  // instance id
  // reserved

  rfpmsg_update_length(oi->ctrl_client->obuf);
  retval = fwd_message_send(oi->ctrl_client);
  return 0;
}
