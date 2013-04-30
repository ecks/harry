#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/param.h>
#include <unistd.h>

#include "log.h"
#include "vty.h"
#include "vector.h"
#include "command.h"
 
/* Command vector which includes some level of command lists. Normally
   each daemon maintains each own cmdvec. */
vector cmdvec = NULL;

/* Host information structure. */
struct host host;

static struct cmd_node config_node =
{
  CONFIG_NODE,
  "%s(config)# ",
  1
};


/* Install top node of command vector. */
void
install_node (struct cmd_node *node, 
	      int (*func) (struct vty *))
{
  vector_set_index (cmdvec, node->node, node);
  node->func = func;
  node->cmd_vector = vector_init (VECTOR_MIN_SIZE);
}

/* Breaking up string into each command piece. I assume given
   character is separated by a space character. Return value is a
   vector which includes char ** data element. */
vector
cmd_make_strvec (const char *string)
{
  const char *cp, *start;
  char *token;
  int strlen;
  vector strvec;
  
  if (string == NULL)
    return NULL;
  
  cp = string;

  /* Skip white spaces. */
  while (isspace ((int) *cp) && *cp != '\0')
    cp++;

  /* Return if there is only white spaces */
  if (*cp == '\0')
    return NULL;

  if (*cp == '!' || *cp == '#')
    return NULL;

  /* Prepare return vector. */
  strvec = vector_init (VECTOR_MIN_SIZE);

  /* Copy each command piece and set into vector. */
  while (1) 
    {
      start = cp;
      while (!(isspace ((int) *cp) || *cp == '\r' || *cp == '\n') &&
	     *cp != '\0')
	cp++;
      strlen = cp - start;
      token = calloc(1, sizeof(char) * (strlen + 1));
      memcpy (token, start, strlen);
      *(token + strlen) = '\0';
      vector_set (strvec, token);

      while ((isspace ((int) *cp) || *cp == '\n' || *cp == '\r') &&
	     *cp != '\0')
	cp++;

      if (*cp == '\0')
	return strvec;
    }
}

/* Fetch next description.  Used in cmd_make_descvec(). */
static char *
cmd_desc_str (const char **string)
{
  const char *cp, *start;
  char *token;
  int strlen;
  
  cp = *string;

  if (cp == NULL)
    return NULL;

  /* Skip white spaces. */
  while (isspace ((int) *cp) && *cp != '\0')
    cp++;

  /* Return if there is only white spaces */
  if (*cp == '\0')
    return NULL;

  start = cp;

  while (!(*cp == '\r' || *cp == '\n') && *cp != '\0')
    cp++;

  strlen = cp - start;
  token = calloc (1, sizeof(char) * (strlen + 1));
  memcpy (token, start, strlen);
  *(token + strlen) = '\0';

  *string = cp;

  return token;
}

/* New string vector. */
static vector
cmd_make_descvec (const char *string, const char *descstr)
{ 
  int multiple = 0;
  const char *sp;
  char *token;
  int len;
  const char *cp;
  const char *dp;
  vector allvec;
  vector strvec = NULL;
  struct desc *desc;

  cp = string;
  dp = descstr;

  if (cp == NULL)
    return NULL;

  allvec = vector_init (VECTOR_MIN_SIZE);

  while (1)
    {
      while (isspace ((int) *cp) && *cp != '\0')
	cp++;

      if (*cp == '(')
	{
	  multiple = 1;
	  cp++;
	}
      if (*cp == ')')
	{
	  multiple = 0;
	  cp++;
	}
      if (*cp == '|')
	{
	  if (! multiple)
	    {
	      fprintf (stderr, "Command parse error!: %s\n", string);
	      exit (1);
	    }
	  cp++;
	}
      
      while (isspace ((int) *cp) && *cp != '\0')
	cp++;

      if (*cp == '(')
	{
	  multiple = 1;
	  cp++;
	}

      if (*cp == '\0') 
	return allvec;

      sp = cp;

      while (! (isspace ((int) *cp) || *cp == '\r' || *cp == '\n' || *cp == ')' || *cp == '|') && *cp != '\0')
	cp++;

      len = cp - sp;

      token = calloc(1, sizeof(char) * (len + 1));
      memcpy (token, sp, len);
      *(token + len) = '\0';

      desc = calloc (1, sizeof (struct desc));
      desc->cmd = token;
      desc->str = cmd_desc_str (&dp);

      if (multiple)
	{
	  if (multiple == 1)
	    {
	      strvec = vector_init (VECTOR_MIN_SIZE);
	      vector_set (allvec, strvec);
	    }
	  multiple++;
	}
      else
	{
	  strvec = vector_init (VECTOR_MIN_SIZE);
	  vector_set (allvec, strvec);
	}
      vector_set (strvec, desc);
    }
}

/* Count mandantory string vector size.  This is to determine inputed
   command has enough command length. */
static int
cmd_cmdsize (vector strvec)
{
  unsigned int i;
  int size = 0;
  vector descvec;
  struct desc *desc;

  for (i = 0; i < vector_active (strvec); i++)
    if ((descvec = vector_slot (strvec, i)) != NULL)
    {
      if ((vector_active (descvec)) == 1
        && (desc = vector_slot (descvec, 0)) != NULL)
	{
	  if (desc->cmd == NULL || CMD_OPTION (desc->cmd))
	    return size;
	  else
	    size++;
	}
      else
	size++;
    }
  return size;
}

/* Install a command into a node. */
void
install_element (enum node_type ntype, struct cmd_element *cmd)
{
  struct cmd_node *cnode;
  
  /* cmd_init hasn't been called */
  if (!cmdvec)
    return;
  
  cnode = vector_slot (cmdvec, ntype);

  if (cnode == NULL) 
    {
      fprintf (stderr, "Command node %d doesn't exist, please check it\n",
	       ntype);
      exit (1);
    }

  vector_set (cnode->cmd_vector, cmd);

  if (cmd->strvec == NULL)
    cmd->strvec = cmd_make_descvec (cmd->string, cmd->doc);

  cmd->cmdsize = cmd_cmdsize (cmd->strvec);
}

/* This function write configuration of this host. */
static int
config_write_host (struct vty *vty)
{
  /* don't reall know what this does for now */
}

/* Utility function for getting command vector. */
static vector
cmd_node_vector (vector v, enum node_type ntype)
{
  struct cmd_node *cnode = vector_slot (v, ntype);
  return cnode->cmd_vector;
}

/* Completion match types. */
enum match_type 
{
  no_match,
  extend_match,
  ipv4_prefix_match,
  ipv4_match,
  ipv6_prefix_match,
  ipv6_match,
  range_match,
  vararg_match,
  partly_match,
  exact_match 
};

/* Filter vector by command character with index. */
static enum match_type
cmd_filter_by_string (char *command, vector v, unsigned int index)
{
  unsigned int i;
  const char *str;
 struct cmd_element *cmd_element;
 enum match_type match_type;
 vector descvec;
 struct desc * desc;

 match_type = no_match;

 /* If command and cmd_element string does not match set NULL to vector */
 for (i = 0; i < vector_active (v); i++)
   if ((cmd_element = vector_slot (v, i)) != NULL)
   {
	/* If given index is bigger than max string vector of command,
	   set NULL */
	if (index >= vector_active (cmd_element->strvec))
	  vector_slot (v, i) = NULL;
	else
	{
	  unsigned int j;
	  int matched = 0;

	  descvec = vector_slot (cmd_element->strvec, index);
	  for (j = 0; j < vector_active (descvec); j++)
	    if ((desc = vector_slot (descvec, j)))
            {
	      str = desc->cmd;

	      if (CMD_VARARG (str))
	      {
	        if (match_type < vararg_match)
	          match_type = vararg_match;
		  matched++;
              }
	      else if (CMD_OPTION (str) || CMD_VARIABLE (str))
	      {
	        if (match_type < extend_match)
		  match_type = extend_match;
		  matched++;
	      }
	      else
              {
		if (strcmp (command, str) == 0)
		{
		  match_type = exact_match;
		  matched++;
		}
	      }
            }
          if(!matched)
            vector_slot(v,i) = NULL;
        }
   }
 return match_type;
}
	
/* Check ambiguous match */
static int
is_cmd_ambiguous (char *command, vector v, int index, enum match_type type)
{
  unsigned int i;
  unsigned int j;
  const char *str = NULL;
  struct cmd_element *cmd_element;
  const char *matched = NULL;
  vector descvec;
  struct desc *desc;

  for (i = 0; i < vector_active (v); i++)
    if ((cmd_element = vector_slot (v, i)) != NULL)
    {
      int match = 0;

      descvec = vector_slot (cmd_element->strvec, index);

      for (j = 0; j < vector_active (descvec); j++)
        if ((desc = vector_slot (descvec, j)))
        {
	      enum match_type ret;
	      
	      str = desc->cmd;

	      switch (type)
	      {
		case exact_match:
		  if (!(CMD_OPTION (str) || CMD_VARIABLE (str))
		      && strcmp (command, str) == 0)
		    match++;
		  break;
		case extend_match:
		  if (CMD_OPTION (str) || CMD_VARIABLE (str))
		    match++;
		  break;
	      }
        }
      if (!match)
	  vector_slot (v, i) = NULL;
      }
  return 0;
}

/* Execute command by argument readline. */
int
cmd_execute_command_strict (vector vline, struct vty *vty,
			    struct cmd_element **cmd)
{
  unsigned int i;
  unsigned int index;
  vector cmd_vector;
  enum match_type match = 0;
  struct cmd_element *cmd_element;
  struct cmd_element *matched_element;
  unsigned int matched_count, incomplete_count;
  int argc;
  const char *argv[CMD_ARGC_MAX];
  int varflag;
  char * command;

  cmd_vector = vector_copy(cmd_node_vector(cmdvec, vty->node));

  for(index = 0; index < vector_active(vline); index++)
  { 
    if((command = vector_slot(vline, index)))
    {
      int ret;

      match = cmd_filter_by_string(vector_slot(vline, index), cmd_vector, index);

      if(match == vararg_match)
        break;

      ret = is_cmd_ambiguous (command, cmd_vector, index, match);
      if (ret == 1)
      {
        vector_free (cmd_vector);
        return CMD_ERR_AMBIGUOUS;
      }
      if (ret == 2)
      {
        vector_free (cmd_vector);
        return CMD_ERR_NO_MATCH;
      }
    }
  }

  /* Check matched count. */
  matched_element = NULL;
  matched_count = 0;
  incomplete_count = 0;
  for (i = 0; i < vector_active (cmd_vector); i++)
    if (vector_slot (cmd_vector, i) != NULL)
    {
	cmd_element = vector_slot (cmd_vector, i);

	if (match == vararg_match || index >= cmd_element->cmdsize)
	  {
	    matched_element = cmd_element;
	    matched_count++;
	  }
	else
	  incomplete_count++;
    }

  /* Finish of using cmd_vector. */
  vector_free (cmd_vector);

  /* To execute command, matched_count must be 1. */
  if (matched_count == 0)
    {
      if (incomplete_count)
	return CMD_ERR_INCOMPLETE;
      else
	return CMD_ERR_NO_MATCH;
    }

  if (matched_count > 1)
    return CMD_ERR_AMBIGUOUS;

   /* Argument treatment */
  varflag = 0;
  argc = 0;

  for (i = 0; i < vector_active (vline); i++)
  {
    if (varflag)
      argv[argc++] = vector_slot (vline, i);
    else
    {
      vector descvec = vector_slot (matched_element->strvec, i);

      if (vector_active (descvec) == 1)
      {
        struct desc *desc = vector_slot (descvec, 0);

	if (CMD_VARARG (desc->cmd))
	  varflag = 1;

        if (varflag || CMD_VARIABLE (desc->cmd) || CMD_OPTION (desc->cmd))
		argv[argc++] = vector_slot (vline, i);
      }
      else
	argv[argc++] = vector_slot (vline, i);
    }

    if (argc >= CMD_ARGC_MAX)
      return CMD_ERR_EXEED_ARGC_MAX;
  }

  /* For vtysh execution. */
  if (cmd)
    *cmd = matched_element;

  if (matched_element->daemon)
    return CMD_SUCCESS_DAEMON;

  /* Now execute matched command */
  return (*matched_element->func) (matched_element, vty, argc, argv);
}

/* Configration make from file. */
int
config_from_file (struct vty *vty, FILE *fp)
{
  int ret;
  vector vline;

  while(fgets(vty->buf, VTY_BUFSIZ, fp))
  {
    vline = cmd_make_strvec(vty->buf);

    /* In case of comment line */
    if (vline == NULL)
	continue;
    /* Execute configuration command : this is strict match */
    ret = cmd_execute_command_strict (vline, vty, NULL);


  }

  return CMD_SUCCESS;
}

int
set_log_file(const char *fname, int loglevel)
{
  int ret;
  char *p = NULL;
  const char *fullpath;
  
  /* Path detection. */
  if (! IS_DIRECTORY_SEP (*fname))
    {
      char cwd[MAXPATHLEN+1];
      cwd[MAXPATHLEN] = '\0';
      
      if (getcwd (cwd, MAXPATHLEN) == NULL)
        {
//          zlog_err ("config_log_file: Unable to alloc mem!");
          printf ("config_log_file: Unable to alloc mem!");
          return -1;
        }
      
      if ( (p = calloc (1, strlen (cwd) + strlen (fname) + 2))
          == NULL)
        {
          printf("config_log_file: Unable to alloc mem!");
          return -1;
        }
      sprintf (p, "%s/%s", cwd, fname);
      fullpath = p;
    }
  else
    fullpath = fname;

  ret = zlog_set_file (NULL, fullpath, loglevel);

  if (p)
    free(p);

  if (!ret)
    {
//      vty_out (vty, "can't open logfile %s\n", fname);
      printf("can't open logfile %s\n", fname);
      return -1;
    }

//  if (host.logfile)
//    XFREE (MTYPE_HOST, host.logfile);

//  host.logfile = XSTRDUP (MTYPE_HOST, fname);

  return 1;
}

DEFUN (config_log_file,
       config_log_file_cmd,
       "log file FILENAME",
       "logging control\n"
       "Logging to file\n"
       "Logginig filename\n")
{
  return set_log_file(argv[0], zlog_default->default_lvl);
}

/* Set config filename.  Called from vty.c */
void
host_config_set (char *filename)
{

}

/* Initialize command interface. Install basic nodes and commands. */
void
cmd_init (int terminal)
{
  /* Allocate initial top vector of commands. */
  cmdvec = vector_init (VECTOR_MIN_SIZE);
  
  /* Default host value settings. */
  host.logfile = NULL;
  host.config = NULL;

  install_node (&config_node, config_write_host);

  if(terminal)
  {
    install_element (CONFIG_NODE, &config_log_file_cmd);
  }
}


