#ifndef SISIS_H
#define SISIS_H

int sisis_init(char * sisis_addr, uint64_t host_num, uint64_t ptype);

struct list * get_ctrl_addrs(void);

struct list * get_ctrl_addrs_for_hostnum(unsigned int hostnum);

struct list * get_ospf6_sibling_addrs(void);

unsigned int number_of_sisis_addrs_for_process_type (unsigned int ptype);

#endif
