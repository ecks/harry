#ifndef ZL_LOG
#define ZL_LOG

typedef enum 
{
  ZLOG_NONE,
  ZLOG_DEFAULT,
  ZLOG_ZEBRALITE,
  ZLOG_PUNTER,
  ZLOG_CONTROLLER,
  ZLOG_OSPF_SIBLING
} zlog_proto_t;

/* If maxlvl is set to ZLOG_DISABLED, then no messages will be sent
 *    to that logging destination. */
#define ZLOG_DISABLED   (LOG_EMERG-1)

typedef enum
{
  ZLOG_DEST_SYSLOG = 0,
  ZLOG_DEST_STDOUT,
  ZLOG_DEST_MONITOR,
  ZLOG_DEST_FILE
} zlog_dest_t;
#define ZLOG_NUM_DESTS          (ZLOG_DEST_FILE+1)

struct zlog
{
  const char * ident;
  zlog_proto_t protocol;
  int maxlvl[ZLOG_NUM_DESTS];	/* maximum priority to send to associated
  				   logging destination */
  int default_lvl;
  FILE * fp;
  char * filename;
  int facility;		/* as per syslog facility */
  int record_priority;	/* should messages logged through stdio include the
  			   priority of the message? */
  int syslog_options;
  int timestamp_precision;	/* # of digits of subsecond precision */
};

/* Set logging level for the given destination.  If the log_level
   argument is ZLOG_DISABLED, then the destination is disabled.
   This function should not be used for file logging (use zlog_set_file
   or zlog_reset_file instead). */
extern void zlog_set_level (struct zlog *zl, zlog_dest_t, int log_level);

/* Set logging to the given filename at the specified level. */
extern int zlog_set_file (struct zlog *zl, const char *filename, int log_level);
/* Disable file logging. */
extern int zlog_reset_file (struct zlog *zl);

/* Puts a current timestamp in buf and returns the number of characters
   written (not including the terminating NUL).  The purpose of
   this function is to avoid calls to localtime appearing all over the code.
   It caches the most recent localtime result and can therefore
   avoid multiple calls within the same second.  If buflen is too small,
   *buf will be set to '\0', and 0 will be returned. */
extern size_t zebralite_timestamp(int timestamp_precision /* # subsecond digits */,
			       char *buf, size_t buflen);

/* structure useful for avoiding repeated rendering of the same timestamp */
struct timestamp_control {
   size_t len;		/* length of rendered timestamp */
   int precision;	/* configuration parameter */
   int already_rendered; /* should be initialized to 0 */
   char buf[40];	/* will contain the rendered timestamp */
};

/* Default logging strucutre. */
extern struct zlog *zlog_default;

extern struct zlog * openzlog(const char * progname, zlog_proto_t protocol,
                              int syslog_flags, int syslog_facility);

extern void closezlog(struct zlog * zl);

#endif
