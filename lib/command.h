#ifndef COMMAND_H
#define COMMAND_H

/* Host configuration variable */
struct host
{
  /* Log filename. */
  char *logfile;

  /* config file name of this host */
  char *config;
};

/* There are some command levels which called from command node. */
enum node_type 
{
  CONFIG_NODE,			/* Config node. Default mode of config file. */
  OSPF6_NODE,                   /* OSPF protocol for IPv6 mode */
};

/* Node which has some commands and prompt string and configuration
   function pointer . */
struct cmd_node 
{
  /* Node index. */
  enum node_type node;		

  /* Prompt character at vty interface. */
  const char *prompt;			

  /* Is this node's configuration goes to vtysh ? */
  int vtysh;
  
  /* Node's configuration write function */
  int (*func) (struct vty *);

  /* Vector of this node's command list. */
  vector cmd_vector;	
};

/* Structure of command element. */
struct cmd_element 
{
  const char *string;			/* Command specification by string. */
  int (*func) (struct cmd_element *, struct vty *, int, const char *[]);
  const char *doc;			/* Documentation of this command. */
  int daemon;                   /* Daemon to which this command belong. */
  vector strvec;		/* Pointing out each description vector. */
  unsigned int cmdsize;		/* Command index count. */
  char *config;			/* Configuration string */
  vector subconfig;		/* Sub configuration string */
  u_char attr;			/* Command attributes */
};

/* Command description structure. */
struct desc
{
  char *cmd;                    /* Command string. */
  char *str;                    /* Command's description. */
};



/* Return value of the commands. */
#define CMD_SUCCESS              0
#define CMD_WARNING              1
#define CMD_ERR_NO_MATCH         2
#define CMD_ERR_AMBIGUOUS        3
#define CMD_ERR_INCOMPLETE       4
#define CMD_ERR_EXEED_ARGC_MAX   5
#define CMD_ERR_NOTHING_TODO     6
#define CMD_COMPLETE_FULL_MATCH  7
#define CMD_COMPLETE_MATCH       8
#define CMD_COMPLETE_LIST_MATCH  9
#define CMD_SUCCESS_DAEMON      10

/* Argc max counts. */
#define CMD_ARGC_MAX   25

/* helper defines for end-user DEFUN* macros */
#define DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, attrs, dnum) \
  struct cmd_element cmdname = \
  { \
    .string = cmdstr, \
    .func = funcname, \
    .doc = helpstr, \
    .attr = attrs, \
    .daemon = dnum, \
  };

#define DEFUN_CMD_FUNC_DECL(funcname) \
  static int funcname (struct cmd_element *, struct vty *, int, const char *[]);

#define DEFUN_CMD_FUNC_TEXT(funcname) \
  static int funcname \
    (struct cmd_element *self __attribute__ ((unused)), \
     struct vty *vty __attribute__ ((unused)), \
     int argc __attribute__ ((unused)), \
     const char *argv[] __attribute__ ((unused)) )

#define DEFUN(funcname, cmdname, cmdstr, helpstr) \
  DEFUN_CMD_FUNC_DECL(funcname) \
  DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, 0, 0) \
  DEFUN_CMD_FUNC_TEXT(funcname)

/* Some macroes */
#define CMD_OPTION(S)   ((S[0]) == '[')
#define CMD_VARIABLE(S) (((S[0]) >= 'A' && (S[0]) <= 'Z') || ((S[0]) == '<'))
#define CMD_VARARG(S)   ((S[0]) == '.')

#define CMD_IPV4(S)        ((strcmp ((S), "A.B.C.D") == 0))
#define CMD_IPV4_PREFIX(S) ((strcmp ((S), "A.B.C.D/M") == 0))

/* Common descriptions. */
#define NO_STR "Negate a command or set its defaults\n"
#define DEBUG_STR "Debugging functions (see also 'undebug')\n"
#define ROUTER_STR "Enable a routing process\n"
#define V4NOTATION_STR "specify by IPv4 address notation(e.g. 0.0.0.0)\n"
#define IFNAME_STR "Interface name(e.g. ep0)\n"
#define OSPF6_STR "Open Shortest Path First (OSPF) for IPv6\n"

extern void install_node (struct cmd_node *, int (*) (struct vty *));
extern void install_default (enum node_type);

extern int config_from_file (struct vty *, FILE *);
extern void host_config_set (char *);
int set_log_file(const char *fname, int loglevel);

#endif
