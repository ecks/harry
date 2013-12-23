extern "C" {
#include <stdlib.h>
#include <stdint.h>

#include "dblist.h"
#include "thread.h"
}

struct thread_master * master;
#include "CppUTest/CommandLineTestRunner.h"

int main(int argc, char ** argv)
{
  MemoryLeakWarningPlugin::turnOffNewDeleteOverloads();
  return RUN_ALL_TESTS(argc, argv);
}
