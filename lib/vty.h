#ifndef VTY_H
#define VTY_H

#define VTY_BUFSIZ 512

/* Directory separator. */
#ifndef DIRECTORY_SEP
#define DIRECTORY_SEP '/'
#endif /* DIRECTORY_SEP */

#ifndef IS_DIRECTORY_SEP
#define IS_DIRECTORY_SEP(c) ((c) == DIRECTORY_SEP)
#endif


struct vty
{
  /* File descripter of this vty. */
  int fd;
  
  /* Is this vty connect to file or not */
  enum {VTY_TERM, VTY_FILE, VTY_SHELL, VTY_SHELL_SERV} type;
  
  /* Node status of this vty */
  int node;
  
  /* Command input buffer */
  char *buf;
};

#endif
