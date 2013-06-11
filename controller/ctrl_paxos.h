#ifndef CTRL_PAXOS_H
#define CTRL_PAXOS_H

int ctrl_paxos_init(void);

void ctrl_paxos_deliver(char*, size_t, iid_t, ballot_t, int);

#endif
