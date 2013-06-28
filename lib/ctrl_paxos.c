#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>
#include <netinet/in.h>

#include "util.h"
#include "dblist.h"
#include "rfpbuf.h"
#include "rfp-msgs.h"
#include "libpaxos.h"
#include "paxosflow-common.h"
#include "ctrl_paxos.h"

static paxos_submit_handle * psh = NULL;
static unsigned int src_ptype;
struct list * conns;

enum pf_state
{
  PF_CONNECTING,
  PF_SENT_HELLO,
  PF_RECV_HELLO,
  PF_CONNECTED
};

enum pf_state state;

struct conn_info
{
  enum pf_state state;
  unsigned int dst_ptype;
  struct list node;
};

void ctrl_paxos_new(unsigned int ptype, struct list * dsts)
{
  src_ptype = ptype;
  conns = list_new();
  list_init(conns);

  struct dst_ptype * dst_ptype;
  LIST_FOR_EACH(dst_ptype, struct dst_ptype, node, dsts)
  {
    struct conn_info * conn = calloc(1, sizeof(struct conn_info));
    conn->dst_ptype = dst_ptype->ptype;
    // set state to send hello
    conn->state = PF_CONNECTING;
    list_push_back(conns, &conn->node);
  }
}

void ctrl_paxos_send(unsigned int src, unsigned int dst, enum pfp_type type)
{
  struct rfpbuf * b;
  
  b = paxosflow_alloc(type, PFP10_VERSION, sizeof(struct pfp_header), src, dst);
  pax_submit_nonblock(psh, (char *)rfpbuf_at_assert(b, 0, b->size), b->size);

  rfpbuf_delete(b);

  b = NULL;
}

int ctrl_paxos_init(void)
{
  psh = pax_submit_handle_init();

  if (psh == NULL) 
  {
    printf("Client init failed [submit handle]\n");
    return -1;
  }

  struct rfpbuf * b;

  struct conn_info * conn;
  LIST_FOR_EACH(conn, struct conn_info, node, conns)
  {
    if(conn->state == PF_CONNECTING)
    {
      ctrl_paxos_send(src_ptype, conn->dst_ptype, PFPT_HELLO);
      conn->state = PF_SENT_HELLO;
    }
  }

// TODO:
//  if(number_of_sisis_addrs_for_process_type(SISIS_PTYPE_OSPF6_SBLING) > 0)
//  {
//      ctrl_paxos_send(src_ptype, dst_ptype, PFPT_HELLO);
//  }
//  else
//  {
    // register a callback to send a hello message whenever we see a new ospf sibling come up
//  }

  return 0;
}

void ctrl_paxos_deliver(char * value, size_t val_size, iid_t iid, ballot_t ballot, int proposer)
{
  struct rfpbuf * b;
  const struct pfp_header *ph;

  b = rfpbuf_new(val_size);

  memcpy(b->data, value, val_size);
  b->size += val_size;

  ph = (struct pfp_header *)b->data;

  // only process messages that are destined for this process
  if(ph->dst == src_ptype) 
  {
    struct conn_info * conn;
    LIST_FOR_EACH(conn, struct conn_info, node, conns)
    {
      // find out if we have a connection for the source
      if(ph->src == conn->dst_ptype)
      {
        if(ph->type == PFPT_HELLO) 
        {
          if(conn->state == PF_SENT_HELLO)
          {
            conn->state = PF_CONNECTED; // we are connected if we sent a hello message and receive a hello message
          }
          else if(conn->state == PF_CONNECTED)
          {
            conn->state = PF_RECV_HELLO; // received a hello but have not sent one
          }

          printf("Received PaxosFlow Hello Message\n");
        }
        else
        {
          printf("Received Message\n");
        }
      }
    }
  }
}
