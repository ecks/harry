#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "util.h"
#include "stream-provider.h"

/* State of an active stream.*/
enum stream_state {
    SCS_CONNECTING,             /* Underlying stream is not connected. */
    SCS_CONNECTED,              /* Connection established. */
    SCS_DISCONNECTED            /* Connection failed or connection closed. */
};

static const struct stream_class *stream_classes[] = {
    &tcp_stream_class,
};

static const struct pstream_class *pstream_classes[] = {
    &ptcp_pstream_class,
};

/* Closes 'stream'. */
void
stream_close(struct stream *stream)
{
    if (stream != NULL) {
        char *name = stream->name;
        (stream->class->close)(stream);
        free(name);
    }
}

/* Returns the name of 'stream', that is, the string passed to
 * stream_open(). */
const char *
stream_get_name(const struct stream *stream)
{
    return stream ? stream->name : "(null)";
}

/* Returns the IP address of the peer, or 0 if the peer is not connected over
 * an IP-based protocol or if its IP address is not yet known. */
uint32_t
stream_get_remote_ip(const struct stream *stream)
{
    return stream->remote_ip;
}

/* Returns the transport port of the peer, or 0 if the connection does not
 * contain a port or if the port is not yet known. */
uint16_t
stream_get_remote_port(const struct stream *stream)
{
    return stream->remote_port;
}

/* Returns the IP address used to connect to the peer, or 0 if the connection
 * is not an IP-based protocol or if its IP address is not yet known. */
uint32_t
stream_get_local_ip(const struct stream *stream)
{
    return stream->local_ip;
}

/* Returns the transport port used to connect to the peer, or 0 if the
 * connection does not contain a port or if the port is not yet known. */
uint16_t
stream_get_local_port(const struct stream *stream)
{
    return stream->local_port;
}

static void
scs_connecting(struct stream *stream)
{
    int retval = (stream->class->connect)(stream);
    assert(retval != EINPROGRESS);
    if (!retval) {
        stream->state = SCS_CONNECTED;
    } else if (retval != EAGAIN) {
        stream->state = SCS_DISCONNECTED;
        stream->error = retval;
    }
}

/* Tries to complete the connection on 'stream'.  If 'stream''s connection is
 * complete, returns 0 if the connection was successful or a positive errno
 * value if it failed.  If the connection is still in progress, returns
 * EAGAIN. */
int
stream_connect(struct stream *stream)
{
    enum stream_state last_state;

    do {
        last_state = stream->state;
        switch (stream->state) {
        case SCS_CONNECTING:
            scs_connecting(stream);
            break;

        case SCS_CONNECTED:
            return 0;

        case SCS_DISCONNECTED:
            return stream->error;

        default:
            NOT_REACHED();
        }
    } while (stream->state != last_state);

    return EAGAIN;
}

/* Tries to receive up to 'n' bytes from 'stream' into 'buffer', and returns:
 *
 *     - If successful, the number of bytes received (between 1 and 'n').
 *
 *     - On error, a negative errno value.
 *
 *     - 0, if the connection has been closed in the normal fashion, or if 'n'
 *       is zero.
 *
 * The recv function will not block waiting for a packet to arrive.  If no
 * data have been received, it returns -EAGAIN immediately. */
int
stream_recv(struct stream *stream, void *buffer, size_t n)
{
    int retval = stream_connect(stream);
    printf("stream_recv: %d\n", retval);
    return (retval ? -retval
            : n == 0 ? 0
            : (stream->class->recv)(stream, buffer, n));
}

/* Tries to send up to 'n' bytes of 'buffer' on 'stream', and returns:
 *
 *     - If successful, the number of bytes sent (between 1 and 'n').  0 is
 *       only a valid return value if 'n' is 0.
 *
 *     - On error, a negative errno value.
 *
 * The send function will not block.  If no bytes can be immediately accepted
 * for transmission, it returns -EAGAIN immediately. */
int
stream_send(struct stream *stream, const void *buffer, size_t n)
{
    int retval = stream_connect(stream);
    return (retval ? -retval
            : n == 0 ? 0
            : (stream->class->send)(stream, buffer, n));
}

void
stream_run_wait(struct stream *stream)
{
    if (stream->class->run_wait) {
        (stream->class->run_wait)(stream);
    }
}

/* Arranges for the poll loop to wake up when 'stream' is ready to take an
 * action of the given 'type'. */
void
stream_wait(struct stream *stream, enum stream_wait_type wait)
{
    assert(wait == STREAM_CONNECT || wait == STREAM_RECV
           || wait == STREAM_SEND);

    switch (stream->state) {
    case SCS_CONNECTING:
        wait = STREAM_CONNECT;
        break;

    case SCS_DISCONNECTED:
//        poll_immediate_wake();
        return;
    }
    (stream->class->wait)(stream, wait);
}

void
stream_connect_wait(struct stream *stream)
{
    stream_wait(stream, STREAM_CONNECT);
}

void
stream_recv_wait(struct stream *stream)
{
    stream_wait(stream, STREAM_RECV);
}

void
stream_send_wait(struct stream *stream)
{
    stream_wait(stream, STREAM_SEND);
}

/* Initializes 'stream' as a new stream named 'name', implemented via 'class'.
 * The initial connection status, supplied as 'connect_status', is interpreted
 * as follows:
 *
 *      - 0: 'stream' is connected.  Its 'send' and 'recv' functions may be
 *        called in the normal fashion.
 *
 *      - EAGAIN: 'stream' is trying to complete a connection.  Its 'connect'
 *        function should be called to complete the connection.
 *
 *      - Other positive errno values indicate that the connection failed with
 *        the specified error.
 *
 * After calling this function, stream_close() must be used to destroy
 * 'stream', otherwise resources will be leaked.
 *
 * The caller retains ownership of 'name'. */
void
stream_init(struct stream *stream, const struct stream_class *class,
            int connect_status, const char *name)
{
    memset(stream, 0, sizeof *stream);
    stream->class = class;
    stream->state = (connect_status == EAGAIN ? SCS_CONNECTING
                    : !connect_status ? SCS_CONNECTED
                    : SCS_DISCONNECTED);
    stream->error = connect_status;
    stream->name = xstrdup(name);
    assert(stream->state != SCS_CONNECTING || class->connect);
}

/* Check the validity of the stream class structures. */
static void
check_stream_classes(void)
{
#ifndef NDEBUG
    size_t i;

    for (i = 0; i < ARRAY_SIZE(stream_classes); i++) {
        const struct stream_class *class = stream_classes[i];
        assert(class->name != NULL);
        assert(class->open != NULL);
        if (class->close || class->recv || class->send || class->run
            || class->run_wait || class->wait) {
            assert(class->close != NULL);
            assert(class->recv != NULL);
            assert(class->send != NULL);
            assert(class->wait != NULL);
        } else {
            /* This class delegates to another one. */
        }
    }
#endif
}

/* Given 'name', a stream name in the form "TYPE:ARGS", stores the class
 * named "TYPE" into '*classp' and returns 0.  Returns EAFNOSUPPORT and stores
 * a null pointer into '*classp' if 'name' is in the wrong form or if no such
 * class exists. */
static int
stream_lookup_class(const char *name, const struct stream_class **classp)
{
    size_t prefix_len;
    size_t i;

    check_stream_classes();

    *classp = NULL;
    prefix_len = strcspn(name, ":");
    if (name[prefix_len] == '\0') {
        return EAFNOSUPPORT;
    }
    for (i = 0; i < ARRAY_SIZE(stream_classes); i++) {
        const struct stream_class *class = stream_classes[i];
        if (strlen(class->name) == prefix_len
            && !memcmp(class->name, name, prefix_len)) {
            *classp = class;
            return 0;
        }
    }
    return EAFNOSUPPORT;
}

void
stream_set_remote_ip(struct stream *stream, uint32_t ip)
{
    stream->remote_ip = ip;
}

void
stream_set_remote_port(struct stream *stream, uint16_t port)
{
    stream->remote_port = port;
}

void
stream_set_local_ip(struct stream *stream, uint32_t ip)
{
    stream->local_ip = ip;
}

void
stream_set_local_port(struct stream *stream, uint16_t port)
{
    stream->local_port = port;
}

static int
count_fields(const char *s_)
{
    char *s, *field, *save_ptr;
    int n = 0;

    save_ptr = NULL;
    s = xstrdup(s_);
    for (field = strtok_r(s, ":", &save_ptr); field != NULL;
         field = strtok_r(NULL, ":", &save_ptr)) {
        n++;
    }
    free(s);

    return n;
}

/* Attempts to connect a stream to a remote peer.  'name' is a connection name
 * in the form "TYPE:ARGS", where TYPE is an active stream class's name and
 * ARGS are stream class-specific.
 *
 * Returns 0 if successful, otherwise a positive errno value.  If successful,
 * stores a pointer to the new connection in '*streamp', otherwise a null
 * pointer.  */
int
stream_open(const char *name, struct stream **streamp, uint8_t dscp)
{
    const struct stream_class *class;
    struct stream *stream;
    char *suffix_copy;
    int error;

    /* Look up the class. */
    error = stream_lookup_class(name, &class);
    if (!class) {
        goto error;
    }

    /* Call class's "open" function. */
    suffix_copy = xstrdup(strchr(name, ':') + 1);
    error = class->open(name, suffix_copy, &stream, dscp);
    free(suffix_copy);
    if (error) {
        goto error;
    }

    /* Success. */
    *streamp = stream;
    return 0;

error:
    *streamp = NULL;
    return error;
}

/* Like stream_open(), but for tcp streams the port defaults to
 * 'default_tcp_port' if no port number is given and for SSL streams the port
 * defaults to 'default_ssl_port' if no port number is given. */
int
stream_open_with_default_ports(const char *name_,
                               uint16_t default_tcp_port,
                               uint16_t default_ssl_port,
                               struct stream **streamp,
                               uint8_t dscp)
{
    char *name;
    int error;

    if (!strncmp(name_, "tcp:", 4) && count_fields(name_) < 3) {
        name = xasprintf("%s:%d", name_, default_tcp_port);
    } else if (!strncmp(name_, "ssl:", 4) && count_fields(name_) < 3) {
        name = xasprintf("%s:%d", name_, default_ssl_port);
    } else {
        name = xstrdup(name_);
    }
    error = stream_open(name, streamp, dscp);
    free(name);

    return error;
}

/* Returns the name that was used to open 'pstream'.  The caller must not
 * modify or free the name. */
const char *
pstream_get_name(const struct pstream *pstream)
{
    return pstream->name;
}

/* Given 'name', a pstream name in the form "TYPE:ARGS", stores the class
 * named "TYPE" into '*classp' and returns 0.  Returns EAFNOSUPPORT and stores
 * a null pointer into '*classp' if 'name' is in the wrong form or if no such
 * class exists. */
static int
pstream_lookup_class(const char *name, const struct pstream_class **classp)
{
    size_t prefix_len;
    size_t i;

    check_stream_classes();

    *classp = NULL;
    prefix_len = strcspn(name, ":");
    if (name[prefix_len] == '\0') {
        return EAFNOSUPPORT;
    }
    for (i = 0; i < ARRAY_SIZE(pstream_classes); i++) {
        const struct pstream_class *class = pstream_classes[i];
        if (strlen(class->name) == prefix_len
            && !memcmp(class->name, name, prefix_len)) {
            *classp = class;
            return 0;
        }
    }
    return EAFNOSUPPORT;
}

/* Attempts to start listening for remote stream connections.  'name' is a
 * connection name in the form "TYPE:ARGS", where TYPE is an passive stream
 * class's name and ARGS are stream class-specific.
 *
 * Returns 0 if successful, otherwise a positive errno value.  If successful,
 * stores a pointer to the new connection in '*pstreamp', otherwise a null
 * pointer.  */
int
pstream_open(const char *name, struct pstream **pstreamp, uint8_t dscp)
{
  const struct pstream_class *class;
  struct pstream *pstream;
  char * suffix_copy;
  int error;

  /* Look up the class. */
  error = pstream_lookup_class(name, &class);
  if (!class) {
      goto error;
  }

  /* Call class's "open" function. */
  suffix_copy = xstrdup(strchr(name, ':') + 1);
  error = class->listen(name, suffix_copy, &pstream, dscp);
  free(suffix_copy);
  if (error) {
      goto error;
  }

  /* Success. */
  *pstreamp = pstream;
  return 0;

error:
  *pstreamp = NULL;
  return error;
}

void
pstream_init(struct pstream *pstream, const struct pstream_class *class,
            const char *name)
{
    pstream->class = class;
    pstream->name = xstrdup(name);
}

/* Like pstream_open(), but for ptcp streams the port defaults to
 * 'default_ptcp_port' if no port number is given and for passive SSL streams
 * the port defaults to 'default_pssl_port' if no port number is given. */
int
pstream_open_with_default_ports(const char *name_,
                                uint16_t default_ptcp_port,
                                uint16_t default_pssl_port,
                                struct pstream **pstreamp,
                                uint8_t dscp)
{
    char *name;
    int error;

    if (!strncmp(name_, "ptcp:", 5) && count_fields(name_) < 2) {
        name = xasprintf("%s%d", name_, default_ptcp_port);
    } else if (!strncmp(name_, "pssl:", 5) && count_fields(name_) < 2) {
        name = xasprintf("%s%d", name_, default_pssl_port);
    } else {
        name = xstrdup(name_);
    }
    error = pstream_open(name, pstreamp, dscp);
    free(name);

    return error;
}

/* Tries to accept a new connection on 'pstream'.  If successful, stores the
 * new connection in '*new_stream' and returns 0.  Otherwise, returns a
 * positive errno value.
 *
 * pstream_accept() will not block waiting for a connection.  If no connection
 * is ready to be accepted, it returns EAGAIN immediately. */
int
pstream_accept(struct pstream *pstream, struct stream **new_stream)
{
    int retval = (pstream->class->accept)(pstream, new_stream);
    if (retval) {
        *new_stream = NULL;
    } else {
        assert((*new_stream)->state != SCS_CONNECTING
               || (*new_stream)->class->connect);
    }
    return retval;
}

void
pstream_wait(struct pstream *pstream)
{
    (pstream->class->wait)(pstream);
}
