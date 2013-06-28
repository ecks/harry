#ifndef CTRL_PAXOS_H
#define CTRL_PAXOS_H

struct dst_ptype
{
  unsigned int ptype;
  struct list node;
};

void ctrl_paxos_new(unsigned int, struct list *);

int ctrl_paxos_init(void);

void ctrl_paxos_deliver(char*, size_t, iid_t, ballot_t, int);

#endif
