
#include "DefaultHarnessTests.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <list>
#include <vector>
#include <set>
#include <iterator>
#include <climits>

using namespace std;

// EnqDeqTest methods
void InsertRemoveTest::init(GlobalTestConfig* gtc){
	Rideable* ptr = gtc->allocRideable();
	this->q = dynamic_cast<RContainer*>(ptr);
	if (!q) {
		 errexit("InsertRemoveTest must be run on RContainer type object.");
	}

	gtc->recorder->addThreadField("insOps_total",&Recorder::sumInts);
	gtc->recorder->addThreadField("insOps_stddev",&Recorder::stdDevInts);
	gtc->recorder->addThreadField("insOps_each",&Recorder::concat);
	gtc->recorder->addThreadField("remOps_total",&Recorder::sumInts);
	gtc->recorder->addThreadField("remOps_stddev",&Recorder::stdDevInts);
	gtc->recorder->addThreadField("remOps_each",&Recorder::concat);
	gtc->recorder->addThreadField("remOpsEmpty_total",&Recorder::sumInts);
	gtc->recorder->addThreadField("remOpsEmpty_stddev",&Recorder::stdDevInts);
	gtc->recorder->addThreadField("remOpsEmpty_each",&Recorder::concat);


}

int InsertRemoveTest::execute(GlobalTestConfig* gtc, LocalTestConfig* ltc){
	struct timeval time_up = gtc->finish;
	struct timeval now;
	gettimeofday(&now,NULL);
	int ops = 0;
	int insOps = 0;
	int remOps = 0;
	int remOpsEmpty =0;
	unsigned int r = ltc->seed;
	int tid = ltc->tid;

	while(now.tv_sec < time_up.tv_sec 
		|| (now.tv_sec==time_up.tv_sec && now.tv_usec<time_up.tv_usec) ){
		r = nextRand(r);
	
		if(r%2==0){
			q->insert(ops+1,tid);
			insOps++;
		}
		else{
			if(q->remove(tid)==EMPTY){remOpsEmpty++;}
			remOps++;
		}
		ops++;
		gettimeofday(&now,NULL);
	}

	gtc->recorder->reportThreadInfo("insOps_total",insOps,ltc->tid);
	gtc->recorder->reportThreadInfo("insOps_stddev",insOps,ltc->tid);
	gtc->recorder->reportThreadInfo("insOps_each",insOps,ltc->tid);
	gtc->recorder->reportThreadInfo("remOps_total",remOps,ltc->tid);
	gtc->recorder->reportThreadInfo("remOps_stddev",remOps,ltc->tid);
	gtc->recorder->reportThreadInfo("remOps_each",remOps,ltc->tid);
	gtc->recorder->reportThreadInfo("remOpsEmpty_total",remOpsEmpty,ltc->tid);
	gtc->recorder->reportThreadInfo("remOpsEmpty_stddev",remOpsEmpty,ltc->tid);
	gtc->recorder->reportThreadInfo("remOpsEmpty_each",remOpsEmpty,ltc->tid);

	return ops;
}

void InsertRemoveTest::cleanup(GlobalTestConfig* gtc){

}

// FAITest methods
void FAITest::init(GlobalTestConfig* gtc){
	fai_cntr = 0;
}
int FAITest::execute(GlobalTestConfig* gtc, LocalTestConfig* ltc){
	struct timeval time_up = gtc->finish;
	struct timeval now;
	gettimeofday (&now,NULL);
	int ops = 0;
	while(now.tv_sec < time_up.tv_sec 
		|| (now.tv_sec==time_up.tv_sec && now.tv_usec<time_up.tv_usec) ){
		__sync_fetch_and_add(&(fai_cntr),1);
		ops++;
		gettimeofday (&now,NULL);
	}
	return ops;
}



// NearEmptyTest methods
void NearEmptyTest::init(GlobalTestConfig* gtc){
	Rideable* ptr = gtc->allocRideable();
	this->q = dynamic_cast<RContainer*>(ptr);
	if (!q) {
		 errexit("NearEmptyTest must be run on RContainer type object.");
	}
}
int NearEmptyTest::execute(GlobalTestConfig* gtc, LocalTestConfig* ltc){
	struct timeval time_up = gtc->finish;
	struct timeval now;
	gettimeofday(&now,NULL);
	int ops = 0;
	int tid = ltc->tid;


	while(now.tv_sec < time_up.tv_sec 
		|| (now.tv_sec==time_up.tv_sec && now.tv_usec<time_up.tv_usec) ){

		if(ops%200==0){
			q->insert(ops,tid);
		}
		else{
			q->remove(tid);
		}
		ops++;
		gettimeofday (&now,NULL);
	
	}
	return ops;
}


// AllocatorChurnTest
void AllocatorChurnTest::init(GlobalTestConfig* gtc){
	Rideable* ptr = gtc->allocRideable();
	this->bp = dynamic_cast<RAllocator*>(ptr);
	if (!bp) {
		 errexit("AllocatorChurnTest must be run on RAllocator type object.");
	}
	this->leftovers.resize(gtc->task_num);
}

int AllocatorChurnTest::execute(GlobalTestConfig* gtc, LocalTestConfig* ltc){
	struct timeval time_up = gtc->finish;
	struct timeval now;
	gettimeofday(&now,NULL);
	int ops = 0;
	int tid = ltc->tid;
	list<void*>* allocated = new list<void*>();
	unsigned int r = ltc->seed;

	while(now.tv_sec < time_up.tv_sec 
		|| (now.tv_sec==time_up.tv_sec && now.tv_usec<time_up.tv_usec) ){
		r = nextRand(r);
	
		if(r%2!=0){
			void* ptr = bp->allocBlock(tid);
			allocated->push_back(ptr);
		}
		else if(allocated->empty()==false){
			if(r%4==1){
				void* ptr = allocated->back();
				allocated->pop_back();
				bp->freeBlock(ptr,tid);
			}
			else{
				void* ptr = allocated->front();
				allocated->pop_front();
				bp->freeBlock(ptr,tid);
			}
		}
		ops++;
		gettimeofday(&now,NULL);
	}
	leftovers[tid]=allocated;

	return ops;
}

void AllocatorChurnTest::cleanup(GlobalTestConfig* gtc){

	if(gtc->environment.find("VERIFY") == gtc->environment.end()){
		return;
	}

	bool is_in = false;
	bool passed = true;
	set<void*> s;
	for(int i = 0; i < leftovers.size(); i++){
		for (std::list<void*>::const_iterator iterator = leftovers[i]->begin(), end = leftovers[i]->end(); iterator != end; ++iterator) {
			is_in = (s.find(*iterator) != s.end());
			if(!is_in){
				s.insert(*iterator);
			}
			else{
				cout<<"Verification failed, double allocation: "<<*iterator<<endl;
				passed = false;
			}
		}
	}
	if(passed){
		cout<<"Verification passed!"<<endl;
		gtc->recorder->reportGlobalInfo("notes","verify pass");
	}
	else{
		gtc->recorder->reportGlobalInfo("notes","verify fail");
	}
}


int SequentialTest::execute(GlobalTestConfig* gtc, LocalTestConfig* ltc){
	int tid = ltc->tid;
	if(tid==0){
		return execute(gtc);
	}
	else{
		cout<<"Running sequential test.  Extra threads ignored."<<endl;
		return 0;
	}
}

void SequentialUnitTest::verify(bool stmt){
	if(!stmt){
		passed = false;
	}
	assert(stmt);
}
void SequentialUnitTest::cleanup(GlobalTestConfig* gtc){
	if(passed){
		cout<<"Verification passed!"<<endl;
		gtc->recorder->reportGlobalInfo("notes","verify pass");
	}
	else{
		gtc->recorder->reportGlobalInfo("notes","verify fail");
	}
	this->clean(gtc);
}


UIDGenerator::UIDGenerator(int taskNum){
	this->taskNum=taskNum;
	tidBits = log2(taskNum)+1;
	inc = 1<<tidBits;
}
uint32_t UIDGenerator::initial(int tid){
	return tid+inc;
}
uint32_t UIDGenerator::next(uint32_t prev, int tid){
	uint32_t ret = prev+inc;
	if ((ret | (inc-1))==UINT_MAX){
		ret = 0;
	}
	return ret;
}
uint32_t UIDGenerator::count(uint32_t val){
	return val>>tidBits;	
}
uint32_t UIDGenerator::id(uint32_t val){
	return val%inc;
}









