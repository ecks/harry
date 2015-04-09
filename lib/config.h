#ifndef CONFIG_H
#define CONFIG_H

#define HAVE_IPV6
#define USE_IPV6

#define HAVE_CLOCK_MONOTONIC
#define MAX_PACKET_SIZE 4096

/* Mask for log files */
#define LOGFILE_MASK 0600

#define ZL_SOCKET_PATH "/var/run/zl_client.api"

#define SYSCONFDIR "/etc/zebralite"

// data plane measuremnt
#define DP_MEASUREMNT 0
#endif
