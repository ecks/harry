#include "config.h"

#include "stdlib.h"
#include "stdio.h"
#include "stdbool.h"
#include "string.h"
#include <stddef.h>
#include "errno.h"
#include "sys/un.h"
#include "sys/types.h"
#include "netinet/in.h"

#include "routeflow-common.h"
#include "thread.h"
#include "util.h"
#include "dblist.h"
#include "rfpbuf.h"
#include "zl_serv.h"

enum event {ZL_SERV, ZL_SERV_READ};

static void zl_serv_event(enum event, int, struct zl_serv_cl *);

static int zl_serv_start();

extern struct thread_master * master;

static struct zl_serv * zl_serv_g = NULL;

struct zl_serv * zl_serv_new(void)
{
  struct zl_serv * zl_serv;
  
  zl_serv = calloc(1, sizeof(struct zl_serv));

  return zl_serv;
}

void zl_serv_init(struct zl_serv * zl_serv, struct datapath * dp)
{
  zl_serv->acceptfd = -1;
  list_init(&zl_serv->clients);
  zl_serv->dp = dp;

  zl_serv_g = zl_serv;

  zl_serv_start();
}

/* Make new client */
void zl_serv_create_client(int client_sock, struct zl_serv * zl_serv)
{
  struct zl_serv_cl * client = calloc(1, sizeof(struct zl_serv_cl));

  client->sockfd = client_sock;

//  client->ibuf = rfpbuf_new(MAX_PACKET_SIZE); 
  client->ibuf = NULL;
 
  list_init(&client->node);

  list_push_back(&zl_serv_g->clients, &client->node);
 
  printf("Client sock is %d\n", client_sock);

  zl_serv_event(ZL_SERV_READ, client_sock, client);
}

static int zl_serv_accept(struct thread * thread)
{
  int client_sock, accept_sock;
  int len;
  struct sockaddr_un * client;
  struct zl_serv * zl_serv = THREAD_ARG(thread);

  accept_sock = THREAD_FD(thread);

  /* Reregister */
  zl_serv_event(ZL_SERV, accept_sock, NULL);

  len = sizeof (struct sockaddr_un);
  if((client_sock = accept (accept_sock, (struct sockaddr *) &client, &len)) < 0)
  {    
    fprintf(stderr, "Can't accept zebra socket: %s", strerror (errno));
    return -1;
  }    

  /* Make client socket non-blocking.  */
  set_nonblocking(client_sock);
  
  /* Create new zebra client. */
  zl_serv_create_client (client_sock, zl_serv);

  return 0;
}

static int zl_serv_sock_init(const char * path)
{
  int ret;
  int sock, len;
  struct sockaddr_un serv;
   
  unlink(path);

  if((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
  {
    fprintf(stderr, "Can't create zl_serv socket\n");
  }

    /* Make server socket. */
  memset (&serv, 0, sizeof (struct sockaddr_un));
  serv.sun_family = AF_UNIX;
  strncpy (serv.sun_path, path, strlen (path));
#ifdef HAVE_STRUCT_SOCKADDR_UN_SUN_LEN
  len = serv.sun_len = SUN_LEN(&serv);
#else
  len = sizeof (serv.sun_family) + strlen (serv.sun_path);
#endif /* HAVE_STRUCT_SOCKADDR_UN_SUN_LEN */

  if((ret = bind (sock, (struct sockaddr *) &serv, len)) < 0)
  {
      fprintf (stderr, "Can't bind to unix socket %s: %s",
                 path, strerror (errno));
      close (sock);
      return -1;
  }

  if((ret = listen (sock, 5)) < 0)
  { 
      fprintf(stderr, "Can't listen to unix socket %s: %s",
                 path, strerror (errno));
      close (sock);
      return;
  }

  printf("zl_serv started to listen\n");
  return sock;
}

static int zl_serv_start()
{
  int retval;

  if(zl_serv_g->acceptfd >= 0)
    return 0;

  if((zl_serv_g->acceptfd = zl_serv_sock_init(ZL_SOCKET_PATH)) < 0)
  {
    fprintf(stderr, "zl_serv connection fail\n");
    return -1;
  }

  zl_serv_event(ZL_SERV, zl_serv_g->acceptfd, NULL);

  return 0;
}

void zl_serv_forward(struct rfpbuf * msg)
{
  if(zl_serv_g == NULL)
    return;

  struct zl_serv_cl * client;
  LIST_FOR_EACH(client, struct zl_serv_cl, node, &zl_serv_g->clients)
  {
    client->ibuf = rfpbuf_clone(msg);
    rfpbuf_write(client->ibuf, client->sockfd);
    rfpbuf_delete(client->ibuf);
  }
}

static int zl_serv_read(struct thread * thread)
{
  int sock = THREAD_FD(thread);
  ssize_t nbytes;
  size_t routeflow_len;
  size_t body_len;
  size_t header_len;
  struct zl_serv_cl * zl_serv_cl = THREAD_ARG(thread);
  struct rfp_header * rh;

  zl_serv_cl->t_read = 0;

  printf("read called from zl_serv with fd: %d\n", sock);

  header_len = sizeof(struct rfp_header);
  zl_serv_cl->ibuf = rfpbuf_new(header_len);

  if((nbytes = rfpbuf_read_try(zl_serv_cl->ibuf, sock, header_len)) != header_len) // read header
  {
    printf("Could not read full header\n");
  }

  rh = (struct rfp_header *)zl_serv_cl->ibuf->data; 

  switch (rh->type)
  {
    case RFPT_FORWARD_OSPF6:
    case RFPT_FORWARD_BGP:
      printf("rh type: %d\n", rh->type);
      routeflow_len = ntohs(rh->length);
      body_len = routeflow_len - header_len;
    
      rfpbuf_prealloc_tailroom(zl_serv_cl->ibuf, body_len);

      // read body of data
      rfpbuf_read_try(zl_serv_cl->ibuf, sock, routeflow_len - header_len);
 
      // forward the message
      dp_forward_zl_to_ctrl(zl_serv_g->dp, zl_serv_cl->ibuf);

      // no longer need the data so reset it to NULL -- already freed
      zl_serv_cl->ibuf = NULL;

      break;

    default:
      fprintf(stderr, "Something went wrong\n");
      return -1;
    break;
  }

  zl_serv_event(ZL_SERV_READ, sock, zl_serv_cl); // reregister read

  return 0;
}

static void zl_serv_event(enum event event, int sock, struct zl_serv_cl * zl_serv_client)
{
  switch(event)
  {
    case ZL_SERV:
      thread_add_read(master, zl_serv_accept, zl_serv_client, sock);
      break;

    case ZL_SERV_READ:
      zl_serv_client->t_read = thread_add_read(master, zl_serv_read, zl_serv_client, sock);
      break;

    default:
      break;
  }
}
