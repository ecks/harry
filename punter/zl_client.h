#ifndef ZL_CLIENT
#define ZL_CLIENT

struct zl_client
{
  int sock;
};

extern struct zl_client * zl_client_new();
extern void zl_client_init(struct zl_client * zl_client);

#endif
