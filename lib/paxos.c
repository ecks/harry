#include "rfpbuf.h"
#include "libpaxos.h"
#include "paxos.h"


int rfpbuf_pax_submit(paxos_submit_handle * h, struct rfpbuf * b, size_t size)
{
  return pax_submit_nonblock(psh, rfpbuf_at_assert(b, 0, b->size), b->size);
}
