#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <errno.h>
#include <stddef.h>

#include "compiler.h"
#include "stream.h"
#include "dblist.h"
#include "rfpbuf.h"
#include "rfp-msgs.h"
#include "vconn-provider.h"
#include "util.h"

/* Active stream socket vconn. */

struct vconn_stream
{
    struct vconn vconn;
    struct stream *stream;
    struct rfpbuf *rxbuf;
    struct rfpbuf *txbuf;
    int n_packets;
};

static struct vconn_class stream_vconn_class;

static void vconn_stream_clear_txbuf(struct vconn_stream *);

static struct vconn *
vconn_stream_new(struct stream *stream, int connect_status)
{
    struct vconn_stream *s;

    s = malloc(sizeof *s);
    vconn_init(&s->vconn, &stream_vconn_class, connect_status,
               stream_get_name(stream));
    s->stream = stream;
    s->txbuf = NULL;
    s->rxbuf = NULL;
    s->n_packets = 0;
    s->vconn.remote_ip = stream_get_remote_ip(stream);
    s->vconn.remote_port = stream_get_remote_port(stream);
    s->vconn.local_ip = stream_get_local_ip(stream);
    s->vconn.local_port = stream_get_local_port(stream);
    return &s->vconn;
}

/* Creates a new vconn that will send and receive data on a stream named 'name'
 *  * and stores a pointer to the vconn in '*vconnp'.
 *   *
 *    * Returns 0 if successful, otherwise a positive errno value. */
static int
vconn_stream_open(const char *name, char *suffix OVS_UNUSED,
                  struct vconn **vconnp, uint8_t dscp)
{
    struct stream *stream;
    int error;

    error = stream_open_with_default_ports(name, RFP_TCP_PORT, RFP_SSL_PORT,
                                           &stream, dscp);
    if (!error) {
        error = stream_connect(stream);
        if (!error || error == EAGAIN) {
            *vconnp = vconn_stream_new(stream, error);
            return 0;
        }
    }

    stream_close(stream);
    return error;
}

static struct vconn_stream *
vconn_stream_cast(struct vconn *vconn)
{
    return CONTAINER_OF(vconn, struct vconn_stream, vconn);
}

static void
vconn_stream_close(struct vconn *vconn)
{
    struct vconn_stream *s = vconn_stream_cast(vconn);

//    if ((vconn->error == EPROTO || s->n_packets < 1) && s->rxbuf) {
//        stream_report_content(s->rxbuf->data, s->rxbuf->size, STREAM_OPENFLOW,
//                              THIS_MODULE, vconn_get_name(vconn));
//    }

    stream_close(s->stream);
    vconn_stream_clear_txbuf(s);
    rfpbuf_delete(s->rxbuf);
    free(s);
}

static int
vconn_stream_connect(struct vconn *vconn)
{
    struct vconn_stream *s = vconn_stream_cast(vconn);
    return stream_connect(s->stream);
}

static int
vconn_stream_recv__(struct vconn_stream *s, int rx_len)
{
    struct rfpbuf *rx = s->rxbuf;
    int want_bytes, retval;

    want_bytes = rx_len - rx->size;
    rfpbuf_prealloc_tailroom(rx, want_bytes);
    retval = stream_recv(s->stream, rfpbuf_tail(rx), want_bytes);
    printf("vconn_stream_recv__: %d\n", retval);
    if (retval > 0) {
        rx->size += retval;
        return retval == want_bytes ? 0 : EAGAIN;
    } else if (retval == 0) {
        if (rx->size) {
            printf("connection dropped mid-packet");
            return EPROTO;
        }
        return EOF;
    } else {
        return -retval;
    }
}

static int
vconn_stream_recv(struct vconn *vconn, struct rfpbuf **bufferp)
{
    struct vconn_stream *s = vconn_stream_cast(vconn);
    const struct rfp_header *rh;
    int rx_len;

    /* Allocate new receive buffer if we don't have one. */
    if (s->rxbuf == NULL) {
        s->rxbuf = rfpbuf_new(1564);
    }

    /* Read ofp_header. */
    if (s->rxbuf->size < sizeof(struct rfp_header)) {
        int retval = vconn_stream_recv__(s, sizeof(struct rfp_header));
        printf("vconn_stream_recv: %d\n", retval);
        if (retval) {
            return retval;
        }
    }

    /* Read payload. */
    rh = s->rxbuf->data;
    rx_len = ntohs(rh->length);
    if (rx_len < sizeof(struct rfp_header)) {
        printf("received too-short ofp_header (%d bytes)", rx_len);
        return EPROTO;
    } else if (s->rxbuf->size < rx_len) {
        int retval = vconn_stream_recv__(s, rx_len);
        if (retval) {
            return retval;
        }
    }

    s->n_packets++;
    *bufferp = s->rxbuf;
    s->rxbuf = NULL;
    return 0;
}

static void
vconn_stream_clear_txbuf(struct vconn_stream *s)
{
    rfpbuf_delete(s->txbuf);
    s->txbuf = NULL;
}

static int
vconn_stream_send(struct vconn *vconn, struct rfpbuf *buffer)
{
    struct vconn_stream *s = vconn_stream_cast(vconn);
    ssize_t retval;

    if (s->txbuf) {
        return EAGAIN;
    }

    retval = stream_send(s->stream, buffer->data, buffer->size);
    if (retval == buffer->size) {
        rfpbuf_delete(buffer);
        return 0;
    } else if (retval >= 0 || retval == -EAGAIN) {
//        leak_checker_claim(buffer);
//        s->txbuf = buffer;
//        if (retval > 0) {
//            rfpbuf_pull(buffer, retval);
//        }
        return 0;
    } else {
        return -retval;
    }
}

static void
vconn_stream_run(struct vconn *vconn)
{

}

static void
vconn_stream_run_wait(struct vconn *vconn)
{

}

static void
vconn_stream_wait(struct vconn *vconn, enum vconn_wait_type wait)
{

}

/* Stream-based vconns and pvconns. */

#define STREAM_INIT(NAME)                           \
    {                                               \
            NAME,                                   \
            vconn_stream_open,                      \
            vconn_stream_close,                     \
            vconn_stream_connect,                   \
            vconn_stream_recv,                      \
            vconn_stream_send,                      \
            vconn_stream_run,                       \
            vconn_stream_run_wait,                  \
            vconn_stream_wait,                      \
    }


static struct vconn_class stream_vconn_class = STREAM_INIT("stream");

struct vconn_class tcp_vconn_class = STREAM_INIT("tcp");
