#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "config.h"
#include "log.h"

static int logfile_fd = -1;     /* Used in signal handler. */

struct zlog *zlog_default = NULL;

const char *zlog_proto_names[] = 
{
  "NONE",
  "DEFAULT",
  "ZEBRALITE",
  "PUNTER",
  "CONTROLLER",
  "OSPF-SIBLING",
  NULL,
};

const char *zlog_priority[] =
{
  "emergencies",
  "alerts",
  "critical",
  "errors",
  "warnings",
  "notifications",
  "informational",
  "debugging",
  NULL,
};
 
size_t
quagga_timestamp(int timestamp_precision, char *buf, size_t buflen)
{
  static struct {
    time_t last;
    size_t len;
    char buf[28];
  } cache;
  struct timeval clock;

  /* would it be sufficient to use global 'recent_time' here?  I fear not... */
  gettimeofday(&clock, NULL);

  /* first, we update the cache if the time has changed */
  if (cache.last != clock.tv_sec)
    {
      struct tm *tm;
      cache.last = clock.tv_sec;
      tm = localtime(&cache.last);
      cache.len = strftime(cache.buf, sizeof(cache.buf),
      			   "%Y/%m/%d %H:%M:%S", tm);
    }
  /* note: it's not worth caching the subsecond part, because
     chances are that back-to-back calls are not sufficiently close together
     for the clock not to have ticked forward */

  if (buflen > cache.len)
    {
      memcpy(buf, cache.buf, cache.len);
      if ((timestamp_precision > 0) &&
	  (buflen > cache.len+1+timestamp_precision))
	{
	  /* should we worry about locale issues? */
	  static const int divisor[] = {0, 100000, 10000, 1000, 100, 10, 1};
	  int prec;
	  char *p = buf+cache.len+1+(prec = timestamp_precision);
	  *p-- = '\0';
	  while (prec > 6)
	    /* this is unlikely to happen, but protect anyway */
	    {
	      *p-- = '0';
	      prec--;
	    }
	  clock.tv_usec /= divisor[prec];
	  do
	    {
	      *p-- = '0'+(clock.tv_usec % 10);
	      clock.tv_usec /= 10;
	    }
	  while (--prec > 0);
	  *p = '.';
	  return cache.len+1+timestamp_precision;
	}
      buf[cache.len] = '\0';
      return cache.len;
    }
  if (buflen > 0)
    buf[0] = '\0';
  return 0;
}

/* Utility routine for current time printing. */
static void
time_print(FILE *fp, struct timestamp_control *ctl)
{
  if (!ctl->already_rendered)
    {
      ctl->len = quagga_timestamp(ctl->precision, ctl->buf, sizeof(ctl->buf));
      ctl->already_rendered = 1;
    }
  fprintf(fp, "%s ", ctl->buf);
}

/* va_list version of zlog. */
static void
vzlog (struct zlog *zl, int priority, const char *format, va_list args)
{
  struct timestamp_control tsctl;
  tsctl.already_rendered = 0;

  /* If zlog is not specified, use default one. */
  if (zl == NULL)
    zl = zlog_default;

  /* When zlog_default is also NULL, use stderr for logging. */
  if (zl == NULL)
    {
      tsctl.precision = 0;
      time_print(stderr, &tsctl);
      fprintf (stderr, "%s: ", "unknown");
      vfprintf (stderr, format, args);
      fprintf (stderr, "\n");
      fflush (stderr);

      /* In this case we return at here. */
      return;
    }

  tsctl.precision = zl->timestamp_precision;

  /* File output. */
  if ((priority <= zl->maxlvl[ZLOG_DEST_FILE]) && zl->fp)
    {
      va_list ac;
      time_print (zl->fp, &tsctl);
      if (zl->record_priority)
	fprintf (zl->fp, "%s: ", zlog_priority[priority]);
      fprintf (zl->fp, "%s: ", zlog_proto_names[zl->protocol]);
      va_copy(ac, args);
      vfprintf (zl->fp, format, ac);
      va_end(ac);
      fprintf (zl->fp, "\n");
      fflush (zl->fp);
    }

  /* stdout output. */
  if (priority <= zl->maxlvl[ZLOG_DEST_STDOUT])
    {
      va_list ac;
      time_print (stdout, &tsctl);
      if (zl->record_priority)
	fprintf (stdout, "%s: ", zlog_priority[priority]);
     fprintf (stdout, "%s: ", zlog_proto_names[zl->protocol]);
      va_copy(ac, args);
      vfprintf (stdout, format, ac);
      va_end(ac);
      fprintf (stdout, "\n");
      fflush (stdout);
    }
}

#define ZLOG_FUNC(FUNCNAME,PRIORITY) \
  void \
FUNCNAME(const char *format, ...) \
{ \
    va_list args; \
    va_start(args, format); \
    vzlog (NULL, PRIORITY, format, args); \
    va_end(args); \
}

ZLOG_FUNC(zlog_notice, LOG_NOTICE)

ZLOG_FUNC(zlog_debug, LOG_DEBUG)

struct zlog * openzlog(const char * progname, zlog_proto_t protocol,
                       int syslog_flags, int syslog_facility)
{
  struct zlog * zl;
  u_int i;

  zl = calloc(1, sizeof(struct zlog));

  zl->ident = progname;
  zl->protocol = protocol;
  zl->facility = syslog_facility;
  zl->syslog_options = syslog_flags;

  zl->default_lvl = LOG_DEBUG;

    /* Set default logging levels. */
  for (i = 0; i < sizeof(zl->maxlvl)/sizeof(zl->maxlvl[0]); i++)
    zl->maxlvl[i] = ZLOG_DISABLED;
  zl->maxlvl[ZLOG_DEST_MONITOR] = LOG_DEBUG;
  zl->default_lvl = LOG_DEBUG;

  openlog(progname, syslog_flags, zl->facility);

  return zl;
}

void
zlog_set_level (struct zlog *zl, zlog_dest_t dest, int log_level)
{
  if (zl == NULL)
    zl = zlog_default;

  zl->maxlvl[dest] = log_level;
}

int zlog_set_file (struct zlog *zl, const char *filename, int log_level)
{
  FILE *fp;
  mode_t oldumask;

  /* There is opend file.  */
  zlog_reset_file (zl);

  /* Set default zl. */
  if (zl == NULL)
    zl = zlog_default;

  /* Open file. */
  oldumask = umask (0777 & ~LOGFILE_MASK);
  fp = fopen (filename, "a");
  umask(oldumask);
  if (fp == NULL)
    return 0;

  /* Set flags. */
  zl->filename = strdup (filename);
  zl->maxlvl[ZLOG_DEST_FILE] = log_level;
  zl->fp = fp;
  logfile_fd = fileno(fp);

  return 1;
}

/* Reset opend file. */
int
zlog_reset_file (struct zlog *zl)
{
  if (zl == NULL)
    zl = zlog_default;

  if (zl->fp)
    fclose (zl->fp);
  zl->fp = NULL;
  logfile_fd = -1;
  zl->maxlvl[ZLOG_DEST_FILE] = ZLOG_DISABLED;

  if (zl->filename)
    free (zl->filename);
  zl->filename = NULL;

  return 1;
}
