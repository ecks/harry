#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>

#include "thread.h"
#include "vty.h"
#include "vector.h"
#include "command.h"


/* Master of the threads. */
static struct thread_master *master;

struct vty * vty_new()
{
  struct vty * new = calloc(1, sizeof(struct vty));

//  new->obuf = buffer_new(0);	/* Use default buffer size. */
  new->buf = calloc(1, sizeof(char) * VTY_BUFSIZ);
//  new->max = VTY_BUFSIZ;

  return new;
}
/* Close vty interface.  Warning: call this only from functions that
   will be careful not to access the vty afterwards (since it has
   now been freed).  This is safest from top-level functions (called
   directly by the thread dispatcher). */
void
vty_close (struct vty *vty)
{
 
}

/* Read up configuration file from file_name. */
static void
vty_read_file (FILE *confp)
{
  int ret;
  struct vty * vty;

  vty = vty_new();
  vty->fd = 0;
  vty->type = VTY_TERM;
  vty->node = CONFIG_NODE;

  /* Execute configuration file */
  ret = config_from_file(vty, confp);

  vty_close(vty);

}

/* Read up configuration file from file_name. */
void
vty_read_config (char *config_file,
		 char *config_default_dir)
{
  char cwd[MAXPATHLEN];
  FILE *confp = NULL;
  char *fullpath;
  char *tmp = NULL;

  /* If -f flag specified. */
  if(config_file != NULL)
  {
    if(! IS_DIRECTORY_SEP (config_file[0]))
    {
      getcwd(cwd, MAXPATHLEN);
      tmp = calloc(1, strlen(cwd) + strlen(config_file) + 2);
      sprintf(tmp, "%s/%s", cwd, config_file);
      fullpath = tmp;
    }
    else
      fullpath = config_file;

    confp = fopen(fullpath, "r");

    if (confp == NULL)
    {
      fprintf(stderr, "failed to open configuration file\n");
      exit(1);
    }
  }
  else
  {

  }

  vty_read_file(confp);

  fclose(confp);

  host_config_set(fullpath);
}

void vty_init(struct thread_master * master_thread)
{
  master = master_thread;
}
