#ifndef PAXOS_H
#define PAXOS_H

int rfpbuf_pax_submit(paxos_submit_handle * h, struct rfpbuf * b, size_t size);

#endif
