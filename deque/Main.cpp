#ifndef _REENTRANT
#define _REENTRANT /* basic 3-lines for threads */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include "Harness.hpp" // main harness header
#include "DefaultHarnessTests.hpp"
#include "SGLDeque.hpp"
#include "MMDeque.hpp"
#include "OFDeque.hpp"
#include "FCDeque.hpp"
#include "WSDeque.hpp"
#include "scal-master/src/datastructures/ts_deque.h"

#include "Tests.hpp"

#include <exception>

// local headers

using namespace std;

GlobalTestConfig *gtc;

// the main function
// sets up output and tests
int main(int argc, char *argv[])
{

  gtc = new GlobalTestConfig();

  gtc->addRideableOption(new SGLDequeFactory(), "SGLDeque");
  gtc->addRideableOption(new MMDequeFactory(), "MMDeque");
  gtc->addRideableOption(new FCDequeFactory(), "FCDeque");

  gtc->addRideableOption(new OFDequeFactory<512, true>(), "OFDeque_512");
  gtc->addRideableOption(new OFDequeFactory<1024, true>(), "OFDeque_1024");
  gtc->addRideableOption(new OFDequeFactory<4096, true>(), "OFDeque_4096");
  gtc->addRideableOption(new OFDequeFactory<8192, true>(), "OFDeque_8192");

  gtc->addRideableOption(new OFDequeFactory<512, false>(), "OFDeque_512_NoElim");
  gtc->addRideableOption(new OFDequeFactory<1024, false>(), "OFDeque_1024_NoElim");
  gtc->addRideableOption(new OFDequeFactory<4096, false>(), "OFDeque_4096_NoElim");
  gtc->addRideableOption(new OFDequeFactory<8192, false>(), "OFDeque_8192_NoElim");

  gtc->addRideableOption(new WSDequeFactory(), "WSDeque");
  gtc->addRideableOption(new TSDequeFactory(), "TSDeque-HWClock");
  gtc->addRideableOption(new TSDequeFactory(TSDequeFactory::AtomicCounterTS), "TSDeque-FAI");

  gtc->addTestOption(new FAITest(), "FAI Test");
  gtc->addTestOption(new PotatoTest(0), "PotatoTest(0 ms delay)");
  gtc->addTestOption(new PotatoTest(1), "PotatoTest(1 ms delay)");
  gtc->addTestOption(new PotatoTest(2), "PotatoTest(2 ms delay)");
  gtc->addTestOption(new DequeInsertRemoveTest(), "DequeInsertRemoveTest");
  gtc->addTestOption(new QueueVerificationTest(), "QueueVerificationTest");
  gtc->addTestOption(new StackVerificationTest(), "StackVerificationTest");
  gtc->addTestOption(new DequeLatencyTest(), "DequeLatencyTest");

  try
  {
    gtc->parseCommandLine(argc, argv);
  }
  catch (...)
  {
    return 0;
  }

  if (gtc->verbose)
  {
    fprintf(stdout, "Testing:  %d threads for %lu seconds on %s using %s\n",
            gtc->task_num, gtc->interval, gtc->getTestName().c_str(), gtc->getRideableName().c_str());
  }

  // register fancy seg fault handler to get some
  // info in case of crash
  signal(SIGSEGV, faultHandler);

  // do the work....
  try
  {
    gtc->runTest();
  }
  catch (exception &e)
  {
    printf("%s", e.what());
    return 0;
  }

  // print out results
  if (gtc->verbose)
  {
    printf("Operations/sec: %ld\n", gtc->total_operations / gtc->interval);
  }
  else
  {
    printf("%ld \t", gtc->total_operations / gtc->interval);
  }

  return 0;
}
