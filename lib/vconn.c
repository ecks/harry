#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "util.h"
#include "dblist.h"
#include "rfpbuf.h"
#include "rfp-msgs.h"
#include "rfp-errors.h"
#include "routeflow-common.h"
#include "vconn-provider.h"

/* State of an active vconn.*/
enum vconn_state {
    /* This is the ordinary progression of states. */
    VCS_CONNECTING,             /* Underlying vconn is not connected. */
    VCS_SEND_HELLO,             /* Waiting to send OFPT_HELLO message. */
    VCS_RECV_HELLO,             /* Waiting to receive OFPT_HELLO message. */
    VCS_CONNECTED,              /* Connection established. */

    /* These states are entered only when something goes wrong. */
    VCS_SEND_ERROR,             /* Sending OFPT_ERROR message. */
    VCS_DISCONNECTED            /* Connection failed or connection closed. */
};

static struct vconn_class *vconn_classes[] = {
    &tcp_vconn_class,
};

static int do_recv(struct vconn *, struct rfpbuf **);
static int do_send(struct vconn *, struct rfpbuf *);

/* Check the validity of the vconn class structures. */
static void
check_vconn_classes(void)
{
#ifndef NDEBUG
    size_t i;

    for (i = 0; i < ARRAY_SIZE(vconn_classes); i++) {
        struct vconn_class *class = vconn_classes[i];
        assert(class->name != NULL);
        assert(class->open != NULL);
//        if (class->close || class->recv || class->send
//            || class->run || class->run_wait || class->wait) {
          if (class->run || class->run_wait || class->wait) {
            assert(class->close != NULL);
//            assert(class->recv != NULL);
//            assert(class->send != NULL);
            assert(class->wait != NULL);
        } else {
            /* This class delegates to another one. */
        }
    }
#endif
}

/* Given 'name', a connection name in the form "TYPE:ARGS", stores the class
 * named "TYPE" into '*classp' and returns 0.  Returns EAFNOSUPPORT and stores
 * a null pointer into '*classp' if 'name' is in the wrong form or if no such
 * class eists. */
static int
vconn_lookup_class(const char *name, struct vconn_class **classp)
{
    size_t prefix_len;

    prefix_len = strcspn(name, ":");
    if (name[prefix_len] != '\0') {
        size_t i;

        for (i = 0; i < ARRAY_SIZE(vconn_classes); i++) {
            struct vconn_class *class = vconn_classes[i];
            if (strlen(class->name) == prefix_len
                && !memcmp(class->name, name, prefix_len)) {
                *classp = class;
                return 0;
            }
        }
    }

    *classp = NULL;
    return EAFNOSUPPORT;
}

/* Returns 0 if 'name' is a connection name in the form "TYPE:ARGS" and TYPE is
 * a supported connection type, otherwise EAFNOSUPPORT.  */
int
vconn_verify_name(const char *name)
{
    struct vconn_class *class;
    return vconn_lookup_class(name, &class);
}

/* Attempts to connect to an OpenFlow device.  'name' is a connection name in
 * the form "TYPE:ARGS", where TYPE is an active vconn class's name and ARGS
 * are vconn class-specific.
 *
 * The vconn will automatically negotiate an OpenFlow protocol version
 * acceptable to both peers on the connection.  The version negotiated will be
 * no lower than 'min_version' and no higher than OFP10_VERSION.
 *
 * Returns 0 if successful, otherwise a positive errno value.  If successful,
 * stores a pointer to the new connection in '*vconnp', otherwise a null
 * pointer.  */
int
vconn_open(const char *name, int min_version, struct vconn **vconnp,
           uint8_t dscp)
{
    struct vconn_class *class;
    struct vconn *vconn;
    char *suffix_copy;
    int error;

    check_vconn_classes();

    /* Look up the class. */
    error = vconn_lookup_class(name, &class);
    if (!class) {
        goto error;
    }

    /* Call class's "open" function. */
    suffix_copy = xstrdup(strchr(name, ':') + 1);
    error = class->open(name, suffix_copy, &vconn, dscp);
    free(suffix_copy);
    if (error) {
        goto error;
    }

    /* Success. */
    assert(vconn->state != VCS_CONNECTING || vconn->class->connect);
    vconn->min_version = min_version;
    *vconnp = vconn;
    return 0;

error:
    *vconnp = NULL;
    return error;
}

static void
vcs_connecting(struct vconn *vconn)
{
    int retval = (vconn->class->connect)(vconn);
    assert(retval != EINPROGRESS);
    if (!retval) {
        vconn->state = VCS_SEND_HELLO;
    } else if (retval != EAGAIN) {
        vconn->state = VCS_DISCONNECTED;
        vconn->error = retval;
    }
}

static void
vcs_send_hello(struct vconn *vconn)
{
    struct rfpbuf *b;
    int retval;

    b = routeflow_alloc(RFPT_HELLO, RFP10_VERSION, sizeof(struct rfp_header));
    retval = do_send(vconn, b);
    if (!retval) {
        vconn->state = VCS_RECV_HELLO;
    } else {
        rfpbuf_delete(b);
        if (retval != EAGAIN) {
            vconn->state = VCS_DISCONNECTED;
            vconn->error = retval;
        }
    }
}

static void
vcs_recv_hello(struct vconn *vconn)
{
    struct rfpbuf *b;
    int retval;

    retval = do_recv(vconn, &b);
    if (!retval) {
        const struct rfp_header *rh = b->data;
//        enum rfptype type;
//        enum rfperr error;
       
//        error = rfptype_decode(&type, b->data);
//        if (!error && type == RFPTYPE_HELLO) {
        if (rh->type == RFPT_HELLO) {
            if (b->size > sizeof *rh) {
              printf("extra-long hello\n");
            }

            vconn->version = MIN(RFP10_VERSION, rh->version);
            if (vconn->version < vconn->min_version) {
              vconn->state = VCS_SEND_ERROR;
            } else {
              printf("Received RouteFlow version 0x%02x\n", rh->version);
              vconn->state = VCS_CONNECTED;
            }
            rfpbuf_delete(b);
            return;
        } else {
            printf("received message while expecting hello\n");
            retval = EPROTO;
            rfpbuf_delete(b);
        }

    }

    if (retval != EAGAIN) {
      vconn->state = VCS_DISCONNECTED;
      vconn->error = retval == EOF ? ECONNRESET : retval;
    }
}

/* Tries to complete the connection on 'vconn'. If 'vconn''s connection is
 *  * complete, returns 0 if the connection was successful or a positive errno
 *   * value if it failed.  If the connection is still in progress, returns
 *    * EAGAIN. */
int
vconn_connect(struct vconn *vconn)
{
    enum vconn_state last_state;

    assert(vconn->min_version > 0);
    do {
        last_state = vconn->state;
        switch (vconn->state) {
        case VCS_CONNECTING:
            vcs_connecting(vconn);
            break;

        case VCS_SEND_HELLO:
            vcs_send_hello(vconn);
            break;

        case VCS_RECV_HELLO:
            vcs_recv_hello(vconn);
            break;

        case VCS_CONNECTED:
            return 0;

        case VCS_SEND_ERROR:
//            vcs_send_error(vconn);
            break;

        case VCS_DISCONNECTED:
            return vconn->error;

        default:
            NOT_REACHED();
        }
    } while (vconn->state != last_state);

    return EAGAIN;
}

/* Tries to receive an OpenFlow message from 'vconn', which must be an active
 * vconn.  If successful, stores the received message into '*msgp' and returns
 * 0.  The caller is responsible for destroying the message with
 * ofpbuf_delete().  On failure, returns a positive errno value and stores a
 * null pointer into '*msgp'.  On normal connection close, returns EOF.
 *
 * vconn_recv will not block waiting for a packet to arrive.  If no packets
 * have been received, it returns EAGAIN immediately. */
int
vconn_recv(struct vconn *vconn, struct rfpbuf **msgp)
{
    int retval = vconn_connect(vconn);
    if (!retval) {
        retval = do_recv(vconn, msgp);
    }
    return retval;
}

/* Tries to queue 'msg' for transmission on 'vconn'.  If successful, returns 0, * in which case ownership of 'msg' is transferred to the vconn.  Success does
 * not guarantee that 'msg' has been or ever will be delivered to the peer,
 * only that it has been queued for transmission.
 *
 * Returns a positive errno value on failure, in which case the caller
 * retains ownership of 'msg'.
 *
 * vconn_send will not block.  If 'msg' cannot be immediately accepted for
 * transmission, it returns EAGAIN immediately. */
int
vconn_send(struct vconn *vconn, struct rfpbuf *msg)
{
    int retval = vconn_connect(vconn);
    if (!retval) {
        retval = do_send(vconn, msg);
    }
    return retval;
}

static int
do_send(struct vconn *vconn, struct rfpbuf *msg)
{
    int retval;

    assert(msg->size >= sizeof(struct rfp_header));

    rfpmsg_update_length(msg);
    retval = (vconn->class->send)(vconn, msg);
    
    return retval;
}

static int
do_recv(struct vconn *vconn, struct rfpbuf **msgp)
{
    int retval = (vconn->class->recv)(vconn, msgp);
    if (!retval) {
//            char *s = rfp_to_string((*msgp)->data, (*msgp)->size, 1);
//            printf("%s: received: %s", vconn->name, s);
            printf("%s: received msg\n", vconn->name);
//            free(s);
    }
    return retval;
}

/* Closes 'vconn'. */
void
vconn_close(struct vconn *vconn)
{
    if (vconn != NULL) {
        char *name = vconn->name;
        (vconn->class->close)(vconn);
        free(name);
    }
}

/* Initializes 'vconn' as a new vconn named 'name', implemented via 'class'.
 * The initial connection status, supplied as 'connect_status', is interpreted
 * as follows:
 *
 *      - 0: 'vconn' is connected.  Its 'send' and 'recv' functions may be
 *        called in the normal fashion.
 *
 *      - EAGAIN: 'vconn' is trying to complete a connection.  Its 'connect'
 *        function should be called to complete the connection.
 *
 *      - Other positive errno values indicate that the connection failed with
 *        the specified error.
 *
 * After calling this function, vconn_close() must be used to destroy 'vconn',
 * otherwise resources will be leaked.
 *
 * The caller retains ownership of 'name'. */
void
vconn_init(struct vconn *vconn, struct vconn_class *class, int connect_status,
           const char *name)
{
    vconn->class = class;
    vconn->state = (connect_status == EAGAIN ? VCS_CONNECTING
                    : !connect_status ? VCS_SEND_HELLO
                    : VCS_DISCONNECTED);
    vconn->error = connect_status;
    vconn->version = 0;
    vconn->min_version = 0;
    vconn->remote_ip = 0;
    vconn->remote_port = 0;
    vconn->local_ip = 0;
    vconn->local_port = 0;
    vconn->name = strdup(name);
    assert(vconn->state != VCS_CONNECTING || class->connect);
}

/* Allows 'vconn' to perform maintenance activities, such as flushing output
 * buffers. */
void
vconn_run(struct vconn *vconn)
{
    if (vconn->state == VCS_CONNECTING ||
        vconn->state == VCS_SEND_HELLO ||
        vconn->state == VCS_RECV_HELLO) {
        vconn_connect(vconn);
    }

    if (vconn->class->run) {
        (vconn->class->run)(vconn);
    }
}
