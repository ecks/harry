#ifndef ZL_CLIENT_NETWORK_H
#define ZL_CLIENT_NEWORK_H

int zl_client_sock_init(const char * path);
int zl_client_network_write(struct rfpbuf * buf, unsigned int sock);
int zl_client_network_recv(struct rfpbuf * buf, unsigned int sock, unsigned int size);

#endif
