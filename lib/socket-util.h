#ifndef SOCKET_UTIL_H
#define SOCKET_UTIL_H

#include <netinet/ip.h>

/* Default value of dscp bits for connection between controller and manager.
 * Value of IPTOS_PREC_INTERNETCONTROL = 0xc0 which is defined
 * in <netinet/ip.h> is used. */
#define DSCP_DEFAULT (IPTOS_PREC_INTERNETCONTROL >> 2)

int set_nonblocking(int fd);
int lookup_ip(const char *host_name, struct in_addr *address);
int check_connection_completion(int fd);
int inet_open_active(int style, const char *target, uint16_t default_port,
                     struct sockaddr_in *sinp, int *fdp, uint8_t dscp);
bool inet_parse_passive(const char *target, int default_port,
                        struct sockaddr_in *sinp);
int inet_open_passive(int style, const char *target, int default_port,
                      struct sockaddr_in *sinp, uint8_t dscp);
int inet_open_passive6(int style, const char *target, int default_port,
                       struct sockaddr_in6 *sin6p, uint8_t dscp);
struct addrinfo * host_addrinfo(const char *host, const char *serv, int family, int socktype);

extern int setsockopt_ifindex (int, int, int);
extern int sockopt_reuseaddr (int sock);
#endif
