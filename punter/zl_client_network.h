#ifndef ZL_CLIENT_NETWORK_H
#define ZL_CLIENT_NEWORK_H

int zl_client_sock_init(const char * path);
int zl_client_write(struct rfpbuf * buf, unsigned int sock);

#endif
