#ifndef STREAM_PROVIDER_H
#define STREAM_PROVIDER_H 1

#include <assert.h>
#include <sys/types.h>
#include "stream.h"

/* Active stream connection. */

/* Active stream connection.
 *
 * This structure should be treated as opaque by implementation. */
struct stream {
    const struct stream_class *class;
    int state;
    int error;
    uint32_t remote_ip;
    uint16_t remote_port;
    uint32_t local_ip;
    uint16_t local_port;
    char *name;
};

void stream_init(struct stream *, const struct stream_class *,
                 int connect_status, const char *name);
void stream_set_remote_ip(struct stream *, uint32_t remote_ip);
void stream_set_dscp(struct stream *, uint8_t dscp);
void stream_set_remote_port(struct stream *, uint16_t remote_port);
void stream_set_local_ip(struct stream *, uint32_t local_ip);
void stream_set_local_port(struct stream *, uint16_t local_port);
static inline void stream_assert_class(const struct stream *stream,
                                       const struct stream_class *class)
{
    assert(stream->class == class);
}

struct stream_class {
    /* Prefix for connection names, e.g. "tcp", "ssl", "unix". */
    const char *name;

    /* True if this stream needs periodic probes to verify connectivity.  For
     * streams which need probes, it can take a long time to notice the
     * connection was dropped. */
    bool needs_probes;

    /* Attempts to connect to a peer.  'name' is the full connection name
     * provided by the user, e.g. "tcp:1.2.3.4".  This name is useful for error
     * messages but must not be modified.
     *
     * 'suffix' is a copy of 'name' following the colon and may be modified.
     * 'dscp' is the DSCP value that the new connection should use in the IP
     * packets it sends.
     *
     * Returns 0 if successful, otherwise a positive errno value.  If
     * successful, stores a pointer to the new connection in '*streamp'.
     *
     * The open function must not block waiting for a connection to complete.
     * If the connection cannot be completed immediately, it should return
     * EAGAIN (not EINPROGRESS, as returned by the connect system call) and
     * continue the connection in the background. */
    int (*open)(const char *name, char *suffix, struct stream **streamp,
                uint8_t dscp);

    /* Closes 'stream' and frees associated memory. */
    void (*close)(struct stream *stream);

    /* Tries to complete the connection on 'stream'.  If 'stream''s connection
     * is complete, returns 0 if the connection was successful or a positive
     * errno value if it failed.  If the connection is still in progress,
     * returns EAGAIN.
     *
     * The connect function must not block waiting for the connection to
     * complete; instead, it should return EAGAIN immediately. */
    int (*connect)(struct stream *stream);

    /* Tries to receive up to 'n' bytes from 'stream' into 'buffer', and
     * returns:
     *
     *     - If successful, the number of bytes received (between 1 and 'n').
     *
     *     - On error, a negative errno value.
     *
     *     - 0, if the connection has been closed in the normal fashion.
     *
     * The recv function will not be passed a zero 'n'.
     *
     * The recv function must not block waiting for data to arrive.  If no data
     * have been received, it should return -EAGAIN immediately. */
     ssize_t (*recv)(struct stream *stream, void *buffer, size_t n);

    /* Tries to send up to 'n' bytes of 'buffer' on 'stream', and returns:
     *
     *     - If successful, the number of bytes sent (between 1 and 'n').
     *
     *     - On error, a negative errno value.
     *
     *     - Never returns 0.
     *
     * The send function will not be passed a zero 'n'.
     *
     * The send function must not block.  If no bytes can be immediately
     * accepted for transmission, it should return -EAGAIN immediately. */
    ssize_t (*send)(struct stream *stream, const void *buffer, size_t n);

    /* Allows 'stream' to perform maintenance activities, such as flushing
     * output buffers.
     *
     * May be null if 'stream' doesn't have anything to do here. */
    void (*run)(struct stream *stream);

    /* Arranges for the poll loop to wake up when 'stream' needs to perform
     * maintenance activities.
     *
     * May be null if 'stream' doesn't have anything to do here. */
    void (*run_wait)(struct stream *stream);

    /* Arranges for the poll loop to wake up when 'stream' is ready to take an
     * action of the given 'type'. */
    void (*wait)(struct stream *stream, enum stream_wait_type type);
};

/* Active and passive stream classes. */
extern const struct stream_class tcp_stream_class;
extern const struct stream_class unix_stream_class;
#ifdef HAVE_OPENSSL
extern const struct stream_class ssl_stream_class;
#endif

#endif
