#include "stdlib.h"
#include "stdio.h"
#include "netinet/in.h"

#include "ctrl_client.h"
#include "sibling_ctrl.h"

struct ctrl_client * ctrl_client = NULL;

int recv_features_reply(struct ctrl_client * ctrl_client)
{
  printf("Features reply received\n");
}

int recv_routes_reply(struct ctrl_client * ctrl_client)
{
  printf("Routes reply received\n");
}

void sibling_ctrl_init(struct in6_addr * ctrl_addr)
{
  ctrl_client = ctrl_client_new();
  ctrl_client_init(ctrl_client, ctrl_addr);
  ctrl_client->features_reply = recv_features_reply;
  ctrl_client->routes_reply = recv_routes_reply;
}
