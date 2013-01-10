#ifndef SISIS_H
#define SISIS_H

int sisis_init(uint64_t host_num, uint64_t ptype);

struct in6_addr * get_ctrl_addrs(void);

unsigned int number_of_sisis_addrs_for_process_type (unsigned int ptype);

#endif
