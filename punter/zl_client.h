#ifndef ZL_CLIENT
#define ZL_CLIENT

struct zl_client
{
  struct punter_ctrl * punter_ctrl;

  int sockfd;

  struct thread * t_connect;
  struct thread * t_read;
};

extern struct zl_client * zl_client_new();
extern void zl_client_init(struct zl_client * zl_client, struct punter_ctrl * punter_ctrl);

#endif
