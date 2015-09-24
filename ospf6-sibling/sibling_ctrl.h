#ifndef SIBLING_CTRL_H
#define SIBLING_CTRL_H

enum sc_state_update_req
{
  SCG_CTRL_INT_UP_REQ = 0,
  SCG_CTRL_LEAD_ELECT_RCVD_REQ = 1,
  SCG_LEAD_ELECT_COMPL = 2
};

void sibling_ctrl_init();
bool past_leader_elect_start();
void sibling_ctrl_update_state(enum sc_state_update_req);
timestamp_t sibling_ctrl_ingress_timestamp();

void sibling_ctrl_add_ctrl_client(unsigned int hostnum, char * ifname, char * area);
void sibling_ctrl_set_addresses(struct in6_addr * sibling_ctrl);

void sibling_ctrl_interface_init(struct ospf6_interface * oi);

struct interface * sibling_ctrl_if_lookup_by_name(const char * ifname);
struct interface * sibling_ctrl_if_lookup_by_index(int ifindex);

void sibling_ctrl_route_set(struct ospf6_route * route);
void sibling_ctrl_route_unset(struct ospf6_route * route);

int sibling_ctrl_first_xid_rcvd();

// functions dealing with restart msg queue
timestamp_t sibling_ctrl_first_timestamp_rcvd();
struct list * sibling_ctrl_restart_msg_queue();
size_t sibling_ctrl_restart_msg_queue_num_msgs();
int sibling_ctrl_push_to_restart_msg_queue(struct rfpbuf * msg_rcvd);

#endif
