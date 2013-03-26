#ifndef VCONN_H
#define VCONN_H 1

struct pvconn;
struct vconn;

/* Active vconns: virtual connections to OpenFlow devices. */
int vconn_verify_name(const char *name);
int vconn_open(const char *name, int min_version,
               struct vconn **, uint8_t dscp);
void vconn_close(struct vconn *);
/* Active vconns: virtual connections to OpenFlow devices. */
int vconn_verify_name(const char *name);
int vconn_open(const char *name, int min_version,
               struct vconn **, uint8_t dscp);
void vconn_close(struct vconn *);
const char *vconn_get_name(const struct vconn *);
int vconn_connect(struct vconn *);
int vconn_recv(struct vconn *, struct rfpbuf **);
int vconn_send(struct vconn *, struct rfpbuf *);
void vconn_run(struct vconn *);
void vconn_run_wait(struct vconn *);

enum vconn_wait_type {
    WAIT_CONNECT,
    WAIT_RECV,
    WAIT_SEND
};

void vconn_wait(struct vconn *, enum vconn_wait_type, int (*func)(struct thread *), void *);
int pvconn_open(const char *name, struct pvconn **, uint8_t dscp);
void pvconn_close(struct pvconn *);
int pvconn_accept(struct pvconn *, int min_version, struct vconn **);
void pvconn_wait(struct pvconn *, int (*func)(struct thread *), void *);

#endif
