#ifndef OSPF6_RESTART
#define OSPF6_RESTART

pthread_mutex_t first_xid_mutex;
pthread_mutex_t restart_mode_mutex;
pthread_mutex_t restart_msg_q_mutex;
pthread_mutex_t restarted_first_egress_not_sent;
void ospf6_restart_init(struct ospf6_interface *, bool catchup);

#endif
