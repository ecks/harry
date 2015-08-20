#include <stdio.h>
#include <time.h>

void main(int argc, char argv[])
{
  struct timespec time;

  clock_gettime(CLOCK_REALTIME, &time);
  printf("[%ld.%09ld]: ", time.tv_sec, time.tv_nsec);
}
