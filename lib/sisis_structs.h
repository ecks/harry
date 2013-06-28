/*
 * SIS-IS Structures
 * Stephen Sigwart
 * University of Delaware
 *
 * Some structs copied from zebra's prefix.h
 */

#ifndef _SISIS_STRUCTS_H
#define _SISIS_STRUCTS_H

struct sisis_request_ack_info
{
	short valid;
	unsigned long request_id;
	pthread_mutex_t mutex;
	short flags;
	#define SISIS_REQUEST_ACK_INFO_ACKED				(1<<0)
	#define SISIS_REQUEST_ACK_INFO_NACKED				(1<<1)
};

#ifndef USE_IPV6 /* IPv4 Version */
/* SIS-IS address components */
struct sisis_addr_components
{
        unsigned int ptype;
        unsigned int host_num;
        unsigned int pid;
};
#endif /* IPv4 Version */

#endif
