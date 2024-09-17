#include "ParallelLaunch.hpp"
#include "HarnessUtils.hpp"
#include <atomic>


using namespace std;

// BARRIERS --------------------------------------------

// utility barrier function using pthreads barrier
// for timing the other primitives
pthread_barrier_t pthread_barrier;
void barrier()
{
	pthread_barrier_wait(&pthread_barrier);
}
void initSynchronizationPrimitives(int task_num){
	// create barrier
	pthread_barrier_init(&pthread_barrier, NULL, task_num);
}

// ALARM handler ------------------------------------------
// in case of infinite loop
bool testComplete;
void alarmhandler(int sig){
	if(testComplete==false){
		fprintf(stderr,"Time out error.\n");
		faultHandler(sig);
	}
}


// AFFINITY ----------------------------------------------

// from:
//http://stackoverflow.com/questions/1407786/how-to-set-cpu-affinity-of-a-particular-pthread
int stick_this_thread_to_core(int core_id) {
   int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
   if (core_id >= num_cores)  // core_id = 0, 1, ... n-1 if system has n cores
      return -1;

   cpu_set_t cpuset;
   CPU_ZERO(&cpuset);
   CPU_SET(core_id, &cpuset);

   pthread_t current_thread = pthread_self();    
   return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}




void setAffinity(GlobalTestConfig* gtc, LocalTestConfig* ltc){
	int tid = ltc->tid;
	int c = gtc->affinities[tid];
	stick_this_thread_to_core(c);
	ltc->cpu=c;
}

// TEST EXECUTION ------------------------------
// Initializes any locks or barriers we need for the tests
void initTest(GlobalTestConfig* gtc){
	mlockall(MCL_CURRENT | MCL_FUTURE);
	mallopt(M_TRIM_THRESHOLD, -1);	
  	mallopt(M_MMAP_MAX, 0);
	gtc->test->init(gtc);
	for(int i = 0; i<gtc->allocatedRideables.size() && gtc->environment["report"]=="1"; i++){
		if(Reportable* r = dynamic_cast<Reportable*>(gtc->allocatedRideables[i])){
			r->introduce();
		}
	}
}

// function to call the appropriate test
int executeTest(GlobalTestConfig* gtc, LocalTestConfig* ltc){
	int ops =  gtc->test->execute(gtc,ltc);
	if(ltc->tid==0){
		testComplete = true;
	}
	return ops;
}

// Cleans up test
void cleanupTest(GlobalTestConfig* gtc){
	for(int i = 0; i<gtc->allocatedRideables.size() && gtc->environment["report"]=="1"; i++){
		if(Reportable* r = dynamic_cast<Reportable*>(gtc->allocatedRideables[i])){
			r->conclude();
		}
	}
	gtc->test->cleanup(gtc);
}


// THREAD MANIPULATION ---------------------------------------------------

// Thread manipulation from SOR sample code
// this is the thread main function.  All threads start here after creation
// and continue on to run the specified test
void * thread_main (void *lp)
{
	atomic_thread_fence(std::memory_order::memory_order_acq_rel);
	CombinedTestConfig* ctc = ((CombinedTestConfig *) lp);
	GlobalTestConfig* gtc = ctc->gtc;
	LocalTestConfig* ltc = ctc->ltc;
	int task_id = ltc->tid;
	setAffinity(gtc,ltc);

	barrier(); // barrier all threads before setting times

	if(task_id==0){
        	gettimeofday (&gtc->start, NULL);
        	gtc->finish=gtc->start;
			gtc->finish.tv_sec+=gtc->interval;
	}



	barrier(); // barrier all threads before starting

	/* ------- WE WILL DO ALL OF THE WORK!!! ---------*/
	int ops = executeTest(gtc,ltc);

	// record standard statistics
	__sync_fetch_and_add (&gtc->total_operations, ops);
	gtc->recorder->reportThreadInfo("ops",ops,ltc->tid);
	gtc->recorder->reportThreadInfo("ops_stddev",ops,ltc->tid);
	gtc->recorder->reportThreadInfo("ops_each",ops,ltc->tid);

	barrier(); // barrier all threads at end

	return NULL;
}


// This function creates our threads and sets them loose
void parallelWork(GlobalTestConfig* gtc){

	pthread_attr_t attr;
	pthread_t *threads;
	CombinedTestConfig* ctcs;
	int i;
	int task_num = gtc->task_num;

	// init globals
	initSynchronizationPrimitives(task_num);
	initTest(gtc);
	testComplete = false;

	// initialize threads and arguments ----------------
	ctcs = (CombinedTestConfig *) malloc (sizeof (CombinedTestConfig) * gtc->task_num);
	threads = (pthread_t *) malloc (sizeof (pthread_t) * gtc->task_num);
	if (!ctcs || !threads){ errexit ("out of shared memory"); }
	pthread_attr_init (&attr);
	pthread_attr_setscope (&attr, PTHREAD_SCOPE_SYSTEM);
	//pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN + 1024*1024);
	for (i = 0; i < task_num; i++) {
		ctcs[i].gtc = gtc;
		ctcs[i].ltc = new LocalTestConfig();
		ctcs[i].ltc->tid=i;
		ctcs[i].ltc->seed = rand();
	}

	signal(SIGALRM, &alarmhandler);  // set a signal handler
	if(gtc->timeOut){
		alarm(gtc->interval+10);  // set an alarm for interval+10 seconds from now
	}

	atomic_thread_fence(std::memory_order::memory_order_acq_rel);

	// launch threads -------------
	for (i = 1; i < task_num; i++) {
		pthread_create (&threads[i], &attr, thread_main, &ctcs[i]);
	}
	//pthread_key_create(&thread_id_ptr, NULL);
	thread_main(&ctcs[0]); // start working also


	// All threads working here... ( in thread_main() )
	

	// join threads ------------------
	for (i = 1; i < task_num; i++)
    	pthread_join (threads[i], NULL);

	for (i = 0; i < task_num; i++) {
		delete ctcs[i].ltc;
	}

	free(ctcs);
	free(threads);
	cleanupTest(gtc);
}
































