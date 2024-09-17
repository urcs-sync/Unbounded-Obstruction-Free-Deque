
#ifndef _REENTRANT
#define _REENTRANT		/* basic 3-lines for threads */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include "BlockPool.hpp"
#include "Harness.hpp"

using namespace std;

GlobalTestConfig* gtc;

// the main function
// sets up output and tests
int main(int argc, char *argv[])
{

	gtc = new GlobalTestConfig();

	gtc->addRideableOption(new SGLQueueFactory(), "SGLQueue (default)");
	gtc->addRideableOption(new BlockPoolFactory<int>(), "BlockPool (Nonblocking Allocator)");

	gtc->addTestOption(new InsertRemoveTest(), "InsertRemove Test");
	gtc->addTestOption(new NearEmptyTest(), "NearEmpty Test");
	gtc->addTestOption(new FAITest(), "FAI Test");
	gtc->addTestOption(new AllocatorChurnTest(), "AllocatorChurnTest");

	
	try{
		gtc->parseCommandLine(argc,argv);
	}
	catch(...){
		return 0;
	}

	if(gtc->verbose){
		fprintf(stdout, "Testing:  %d threads for %lu seconds on %s\n",
		  gtc->task_num,gtc->interval,gtc->getTestName().c_str());
	}

	// register fancy seg fault handler to get some
	// info in case of crash
	signal(SIGSEGV, faultHandler);

	// do the work....
	try{
		gtc->runTest();
	}
	catch(...){
		return 0;
	}


	// print out results
	if(gtc->verbose){
		printf("Operations/sec: %ld\n",gtc->total_operations/gtc->interval);
	}
	else{
		printf("%ld \t",gtc->total_operations/gtc->interval);
	}

	return 0;
}

























