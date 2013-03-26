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

  oi->thread_send_hello = (struct thread *)NULL;

  if(oi->interface->state == RFPPS_LINK_DOWN)
  {
    printf("Tried to send hello message on link that is down\n");
    return 0;
  }

  printf("About to send hello message\n");

  /* set next thread */
  oi->thread_send_hello = thread_add_timer(master, ospf6_hello_send, oi, oi->hello_interval);

  oi->ctrl_client->obuf = routeflow_alloc(RFPT_FORWARD_OSPF6, RFP10_VERSION, sizeof(struct rfp_header));

  rfo = oi->ctrl_client->obuf->l2;

  oh = &rfo->ospf6_header;
  oh->type = OSPF6_MESSAGE_TYPE_HELLO;
  oh->length = htons(sizeof(struct ospf6_header));

  rfpmsg_update_length(oi->ctrl_client->obuf);
  retval = fwd_message_send(oi->ctrl_client);
  return 0;
}
