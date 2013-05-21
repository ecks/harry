#ifndef SOCKUNION_H
#define SOCKUNION_H

union sockunion
{
  struct sockaddr sa;
  struct sockaddr_in sin;
#ifdef HAVE_IVP6
  struct sockaddr_in6 sin6;
#endif
};

extern int sockunion_accept (int sock, union sockunion *);

#endif
