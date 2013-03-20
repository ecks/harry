#ifndef ZL_SERV_H
#define ZL_SERV_H

struct zl_serv
{
  int acceptfd;

  struct datapath * dp;
  struct list clients;
};

struct zl_serv_cl
{
  int sockfd;
  struct rfpbuf * ibuf;
  struct list node;
  struct thread * t_read;
};

struct zl_serv * zl_serv_new(void);
void zl_serv_init(struct zl_serv * zl_serv, struct datapath * dp);

#endif
