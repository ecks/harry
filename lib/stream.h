#ifndef STREAM_H
#define STREAM_H 1

#include <stdbool.h>
#include <stdint.h>

struct stream;

void stream_usage(const char *name, bool active, bool passive, bool bootstrap);

/* Bidirectional byte streams. */
int stream_verify_name(const char *name);
int stream_open(const char *name, struct stream **, uint8_t dscp);
int stream_open_block(int error, struct stream **);
void stream_close(struct stream *);
const char *stream_get_name(const struct stream *);
uint32_t stream_get_remote_ip(const struct stream *);
uint16_t stream_get_remote_port(const struct stream *);
uint32_t stream_get_local_ip(const struct stream *);
uint16_t stream_get_local_port(const struct stream *);
int stream_connect(struct stream *);
int stream_recv(struct stream *, void *buffer, size_t n);
int stream_send(struct stream *, const void *buffer, size_t n);

void stream_run(struct stream *);
void stream_run_wait(struct stream *);

enum stream_wait_type {
    STREAM_CONNECT,
    STREAM_RECV,
    STREAM_SEND
};
void stream_wait(struct stream *, enum stream_wait_type);
void stream_connect_wait(struct stream *);
void stream_recv_wait(struct stream *);
void stream_send_wait(struct stream *);

/* Convenience functions. */

int stream_open_with_default_ports(const char *name,
                                   uint16_t default_tcp_port,
                                   uint16_t default_ssl_port,
                                   struct stream **,
                                   uint8_t dscp);
bool stream_parse_target_with_default_ports(const char *target,
                                           uint16_t default_tcp_port,
                                           uint16_t default_ssl_port,
                                           struct sockaddr_in *sin);
int stream_needs_probes(const char *name);

#endif
