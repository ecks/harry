#ifndef VCONN_PROVIDER_H
#define VCONN_PROVIDER_H 1

/* Provider interface to vconns, which provide a virtual connection to an
 * OpenFlow device. */

#include "stdint.h"
#include "routeflow-common.h"
#include "vconn.h"

/* Active virtual connection to an OpenFlow device. */
struct vconn {
    struct vconn_class *class;
    int state;
    int error;
    enum rfp_version min_version;
    enum rfp_version version;
    uint32_t remote_ip;
    uint16_t remote_port;
    uint32_t local_ip;
    uint16_t local_port;
    char *name;
};

void vconn_init(struct vconn *, struct vconn_class *, int connect_status,
                const char *name);
void vconn_set_remote_ip(struct vconn *, uint32_t remote_ip);
void vconn_set_remote_port(struct vconn *, uint16_t remote_port);
void vconn_set_local_ip(struct vconn *, uint32_t local_ip);
void vconn_set_local_port(struct vconn *, uint16_t local_port);
static inline void vconn_assert_class(const struct vconn *vconn,
                                      const struct vconn_class *class)
{
    assert(vconn->class == class);
}

struct vconn_class {
    /* Prefix for connection names, e.g. "nl", "tcp". */
    const char *name;

    /* Attempts to connect to an OpenFlow device.  'name' is the full
     * connection name provided by the user, e.g. "tcp:1.2.3.4".  This name is
     * useful for error messages but must not be modified.
     *
     * 'suffix' is a copy of 'name' following the colon and may be modified.
     * 'dscp' is the DSCP value that the new connection should use in the IP
     * packets it sends.
     *
     * Returns 0 if successful, otherwise a positive errno value.  If
     * successful, stores a pointer to the new connection in '*vconnp'.
     *
     * The open function must not block waiting for a connection to complete.
     * If the connection cannot be completed immediately, it should return
     * EAGAIN (not EINPROGRESS, as returned by the connect system call) and
     * continue the connection in the background. */
    int (*open)(const char *name, char *suffix, struct vconn **vconnp,
                uint8_t dscp);

    /* Closes 'vconn' and frees associated memory. */
    void (*close)(struct vconn *vconn);

    /* Tries to complete the connection on 'vconn'.  If 'vconn''s connection is
     * complete, returns 0 if the connection was successful or a positive errno
     * value if it failed.  If the connection is still in progress, returns
     * EAGAIN.
     *
     * The connect function must not block waiting for the connection to
     * complete; instead, it should return EAGAIN immediately. */
    int (*connect)(struct vconn *vconn);

    /* Tries to receive an OpenFlow message from 'vconn'.  If successful,
     * stores the received message into '*msgp' and returns 0.  The caller is
     * responsible for destroying the message with ofpbuf_delete().  On
     * failure, returns a positive errno value and stores a null pointer into
     * '*msgp'.
     *
     * If the connection has been closed in the normal fashion, returns EOF.
     *
     * The recv function must not block waiting for a packet to arrive.  If no
     * packets have been received, it should return EAGAIN. */
    int (*recv)(struct vconn *vconn, struct rfpbuf **msgp);

    /* Tries to queue 'msg' for transmission on 'vconn'.  If successful,
     * returns 0, in which case ownership of 'msg' is transferred to the vconn.
     * Success does not guarantee that 'msg' has been or ever will be delivered
     * to the peer, only that it has been queued for transmission.
     *
     * Returns a positive errno value on failure, in which case the caller
     * retains ownership of 'msg'.
     *
     * The send function must not block.  If 'msg' cannot be immediately
     * accepted for transmission, it should return EAGAIN. */
    int (*send)(struct vconn *vconn, struct rfpbuf *msg);

    /* Allows 'vconn' to perform maintenance activities, such as flushing
     * output buffers.
     *
     * May be null if 'vconn' doesn't have anything to do here. */
    void (*run)(struct vconn *vconn);

    /* Arranges for the poll loop to wake up when 'vconn' needs to perform
     * maintenance activities.
     *
     * May be null if 'vconn' doesn't have anything to do here. */
    void (*run_wait)(struct vconn *vconn);

    /* Arranges for the poll loop to wake up when 'vconn' is ready to take an
     * action of the given 'type'. */
    void (*wait)(struct vconn *vconn, enum vconn_wait_type type);
};

/* Passive virtual connection to an OpenFlow device.
 *
 * This structure should be treated as opaque by vconn implementations. */
struct pvconn {
    struct pvconn_class *class;
    char *name;
}; 

void pvconn_init(struct pvconn *, struct pvconn_class *, const char *name);

struct pvconn_class {
    /* Prefix for connection names, e.g. "ptcp", "pssl". */
    const char *name;

    /* Attempts to start listening for OpenFlow connections.  'name' is the
     * full connection name provided by the user, e.g. "ptcp:1234".  This name
     * is useful for error messages but must not be modified.
     *
     * 'suffix' is a copy of 'name' following the colon and may be modified.
     * 'dscp' is the DSCP value that the new connection should use in the IP
     * packets it sends.
     *
     * Returns 0 if successful, otherwise a positive errno value.  If
     * successful, stores a pointer to the new connection in '*pvconnp'.
     *
     * The listen function must not block.  If the connection cannot be
     * completed immediately, it should return EAGAIN (not EINPROGRESS, as
     * returned by the connect system call) and continue the connection in the
     * background. */
    int (*listen)(const char *name, char *suffix, struct pvconn **pvconnp,
                  uint8_t dscp);

    /* Closes 'pvconn' and frees associated memory. */
    void (*close)(struct pvconn *pvconn);

    /* Tries to accept a new connection on 'pvconn'.  If successful, stores the
     * new connection in '*new_vconnp' and returns 0.  Otherwise, returns a
     * positive errno value.
     *
     * The accept function must not block waiting for a connection.  If no
     * connection is ready to be accepted, it should return EAGAIN. */
    int (*accept)(struct pvconn *pvconn, struct vconn **new_vconnp);

    /* Arranges for the poll loop to wake up when a connection is ready to be
     * accepted on 'pvconn'. */
    void (*wait)(struct pvconn *pvconn);
};

/* Active and passive vconn classes. */
extern struct vconn_class tcp_vconn_class;
extern struct pvconn_class ptcp_pvconn_class;

#endif
