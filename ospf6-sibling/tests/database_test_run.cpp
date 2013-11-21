#include "CppUTest/CommandLineTestRunner.h"

int main(int argc, char ** argv)
{
  MemoryLeakWarningPlugin::turnOffNewDeleteOverloads();
  return RUN_ALL_TESTS(argc, argv);
}
