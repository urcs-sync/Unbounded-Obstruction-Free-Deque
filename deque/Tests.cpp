
#include "Tests.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <climits>
#include "OFDeque.hpp"

using namespace std;

// PotatoTest methods
void PotatoTest::init(GlobalTestConfig* gtc){
	Rideable* ptr = gtc->allocRideable();
	this->q = dynamic_cast<RContainer*>(ptr);
	if (!q) {
		errexit("PotatoTest must be run on RQueue or RDualQueue type object.");
	}
	if (gtc->verbose) {
		cout<<"Running PotatoTest on total container."<<endl;
	}
	gtc->recorder->addThreadField("insOps",&Recorder::sumInts);
	gtc->recorder->addThreadField("insOps_stddev",&Recorder::stdDevInts);
	gtc->recorder->addThreadField("insOps_each",&Recorder::concat);
	gtc->recorder->addThreadField("remOps",&Recorder::sumInts);
	gtc->recorder->addThreadField("remOps_stddev",&Recorder::stdDevInts);
	gtc->recorder->addThreadField("remOps_each",&Recorder::concat);
	ug = new UIDGenerator(gtc->task_num);
}

int PotatoTest::execute(GlobalTestConfig* gtc, LocalTestConfig* ltc){
	return executeQueue(gtc,ltc);
}

int PotatoTest::executeQueue(GlobalTestConfig* gtc, LocalTestConfig* ltc){

	struct timeval time_up = gtc->finish;
	struct timeval now;
	gettimeofday(&now,NULL);
	int ops = 0;

	int insOps = 0;
	int remOps = 0;

	unsigned int r = ltc->seed;
	int tid = ltc->tid;
	bool hot = false;
	int j;

	if(tid == 0){
		hot = true;
	}
	
	int32_t inserting = ug->initial(tid);


	while(now.tv_sec < time_up.tv_sec 
		|| (now.tv_sec==time_up.tv_sec && now.tv_usec<time_up.tv_usec) ){
		r = nextRand(r);

		if(hot || r%2==0){
			insOps++;
			inserting = ug->next(inserting,tid);
			if(inserting==0){
				cout<<"Overflow on thread "<<tid<<". Terminating its execution."<<endl;
				break;
			}
		}

		if(hot){
			usleep(hotPotatoPenalty);
			q->insert(-1*inserting,tid);
			hot = false;
			insOps++;
		}
		else if(r%2==0){
			q->insert(inserting,tid);
			insOps++;
		}
		else{
			j=EMPTY;
			while(j==EMPTY){
				j=q->remove(tid);
			}
			if(j<0){
				hot = true;
			}
			remOps++;
		}
		ops++;
		gettimeofday(&now,NULL);
	}
	inserting = ug->next(inserting,tid);
	q->insert(inserting,tid);

	gtc->recorder->reportThreadInfo("insOps",insOps,ltc->tid);
	gtc->recorder->reportThreadInfo("insOps_stddev",insOps,ltc->tid);
	gtc->recorder->reportThreadInfo("insOps_each",insOps,ltc->tid);
	gtc->recorder->reportThreadInfo("remOps",remOps,ltc->tid);
	gtc->recorder->reportThreadInfo("remOps_stddev",remOps,ltc->tid);
	gtc->recorder->reportThreadInfo("remOps_each",remOps,ltc->tid);

	return ops;

}

void PotatoTest::cleanup(GlobalTestConfig* gtc) {}


// QueueVerificationTest methods
void QueueVerificationTest::init(GlobalTestConfig* gtc) {
	Rideable* ptr = gtc->allocRideable();
	this->q = dynamic_cast<RDeque*>(ptr);
	if (!q) {
		cout<<"QueueVerificationTest should be run on RDeque type object."<<endl;
		errexit("QueueVerificationTest must be run on RDeque type object.");
	}
	
	ug = new UIDGenerator(gtc->task_num);
	passed.store(1);
}

int QueueVerificationTest::execute(GlobalTestConfig* gtc, LocalTestConfig* ltc){

	struct timeval time_up = gtc->finish;
	struct timeval now;
	gettimeofday(&now,NULL);
	int ops = 0;
	unsigned int r = ltc->seed;
	int tid = ltc->tid;

	vector<uint32_t> found;
	found.resize(gtc->task_num);
	for(int i = 0; i<gtc->task_num; i++){
		found[i]=0;
	}

	uint32_t inserting = ug->initial(tid);

	while(now.tv_sec < time_up.tv_sec 
		|| (now.tv_sec==time_up.tv_sec && now.tv_usec<time_up.tv_usec) ){
		r = nextRand(r);
	
		if(r%2==0){
			q->right_push(inserting,tid);
			inserting = ug->next(inserting,tid);
			if(inserting==0){
				cout<<"Overflow on thread "<<tid<<". Terminating its execution."<<endl;
				break;
			}

		}
		else{
			uint32_t removed = (uint32_t) q->left_pop(tid);
			if(removed==EMPTY){continue;}
			uint32_t id = ug->id(removed);
			uint32_t cnt = ug->count(removed);

			if(cnt<=found[id]){
				cout<<"Verification failed! Reordering violation."<<endl;
				cout<<"Im thread "<<tid<<endl;
				cout<<"Found "<<found[id]<<" for thread "<<id<<endl;
				cout<<"Putting "<<cnt<<endl;
				passed.store(0);	
				assert(false);
			}
			else{
				found[id]=cnt;
			}
		}
		ops++;

		gettimeofday(&now,NULL);
	}
	return ops;
}


void QueueVerificationTest::cleanup(GlobalTestConfig* gtc){
	if(passed){
		cout<<"Verification passed!"<<endl;
		gtc->recorder->reportGlobalInfo("notes","verify pass");
	}
	else{
		gtc->recorder->reportGlobalInfo("notes","verify fail");
	}
}

void StackVerificationTest::barrier() {
	pthread_barrier_wait(&pthread_barrier);
}

void StackVerificationTest::init(GlobalTestConfig* gtc) {
	Rideable* ptr = gtc->allocRideable();
	this->q = dynamic_cast<RDeque*>(ptr);
	if (!q) {
		cout<<"StackVerificationTest should be run on RDeque type object."<<endl;
		errexit("StackVerificationTest must be run on RDeque type object.");
	}

	gtc->recorder->addThreadField("insOps",&Recorder::sumInts);
	gtc->recorder->addThreadField("insOps_stddev",&Recorder::stdDevInts);
	gtc->recorder->addThreadField("insOps_each",&Recorder::concat);
	gtc->recorder->addThreadField("remOps",&Recorder::sumInts);
	gtc->recorder->addThreadField("remOps_stddev",&Recorder::stdDevInts);
	gtc->recorder->addThreadField("remOps_each",&Recorder::concat);
	ug = new UIDGenerator(gtc->task_num);
	pthread_barrier_init(&pthread_barrier, NULL, gtc->task_num);
	
	opsPerPhase = 5000;
	phaseCount.store(0);
	done.store(false);
}

int StackVerificationTest::execute(GlobalTestConfig* gtc, LocalTestConfig* ltc) {

	struct timeval time_up = gtc->finish;
	struct timeval now;
	gettimeofday(&now,NULL);
	int ops = 0;
	unsigned int r = ltc->seed;
	int tid = ltc->tid;
	int phase = 0;

	vector<uint32_t> found;
	found.resize(gtc->task_num);
	for(int i = 0; i<gtc->task_num; i++){
		found[i]=0;
	}

	uint32_t inserting = ug->initial(tid);
	bool empty = true;
	int count = opsPerPhase;

	while(true){
	
		// barrier
		if((phase%2==0 && empty) || (phase%2==1 && count>=opsPerPhase)){
			for(int i = 0; i<gtc->task_num; i++){
				found[i]=0;
			}
			barrier();
			phaseCount.fetch_add(count);
			barrier();
			if(tid==0){
				cout<<"On phase:"<<phase<<" did "<<phaseCount<<endl;
				//assert(phaseCount==gtc->task_num*opsPerPhase || phase==0); 
				phaseCount.store(0);
				done = empty==true && !(now.tv_sec < time_up.tv_sec || (now.tv_sec==time_up.tv_sec && now.tv_usec<time_up.tv_usec));
			}
			count = 0;
			phase++;
			barrier();
			if(done){break;}
			barrier();
		}

		if(phase%2==1){
			q->right_push(inserting,tid);
			inserting = ug->next(inserting,tid);
			if(inserting==0){
				cout<<"Overflow on thread "<<tid<<". Terminating its execution."<<endl;
				break;
			}
			empty = false;
			count++;
		}
		else{
			uint32_t removed = (uint32_t) q->right_pop(tid);
			if(removed==EMPTY){empty=true; continue;}
			uint32_t id = ug->id(removed);
			uint32_t cnt = ug->count(removed);

			if(found[id]!=0 && cnt>=found[id]){
				cout<<"Verification failed! Reordering violation."<<endl;
				cout<<"Im thread "<<tid<<endl;
				cout<<"Found "<<found[id]<<" for thread "<<id<<endl;
				cout<<"Putting "<<cnt<<endl;
				passed.store(0);	
				assert(false);
			}
			else{
				found[id]=cnt;
			}
			count++;
		}
		ops++;

		gettimeofday(&now,NULL);
	}
	cout<<"phases="<<phase<<endl;
	return ops;
}

void DequeInsertRemoveTest::init(GlobalTestConfig* gtc){
	Rideable* ptr = gtc->allocRideable();
	this->q = dynamic_cast<RDeque*>(ptr);
	if (!q) {
		 errexit("DequeInsertRemoveTest must be run on RDeque type object.");
	}

	std::map<std::string,std::string>::iterator it;
    it = gtc->environment.find("access_type");

    if (it == gtc->environment.end()) {
	    this->type = AccessPattern::QUEUE;
        printf("using default access type: QUEUE\n");
    } else {
    	const char *str = it->second.c_str();
    	if (!strcmp(str, "STACK")) {
    		this->type = AccessPattern::STACK;
    	} else if (!strcmp(str, "QUEUE")) {
    	    this->type = AccessPattern::QUEUE;
		} else if (!strcmp(str, "RANDOM")) {
			this->type = AccessPattern::RANDOM;
		} else {
	        printf("unrecognized access pattern - using default access type: QUEUE\n");
		    this->type = AccessPattern::QUEUE;
    	}
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

	this->q->addThreadLogs(gtc->recorder);
}

int DequeInsertRemoveTest::execute(GlobalTestConfig* gtc, LocalTestConfig* ltc){
	struct timeval time_up = gtc->finish;
	struct timeval now;
	gettimeofday(&now,NULL);
	int ops = 0;
	int insOps = 0;
	int remOps = 0;
	int remOpsEmpty = 0;
	unsigned int r = ltc->seed;
	int tid = ltc->tid;

	if (this->type == AccessPattern::QUEUE) {
		while(now.tv_sec < time_up.tv_sec 
			|| (now.tv_sec==time_up.tv_sec && now.tv_usec<time_up.tv_usec) ){
			r = nextRand(r);
		
			if (r%2==0) {
				q->right_push(ops+1,tid);
				insOps++;
			} else {
				if (q->left_pop(tid)==EMPTY) {remOpsEmpty++;}
				remOps++;
			}
			ops++;
			gettimeofday(&now,NULL);
		}
	} else if (this->type == AccessPattern::STACK) {
		while(now.tv_sec < time_up.tv_sec 
		|| (now.tv_sec==time_up.tv_sec && now.tv_usec<time_up.tv_usec) ){
			r = nextRand(r);
	
			if(r%2==0){
				q->right_push(ops+1,tid);
				insOps++;
			} else{
				if(q->right_pop(tid)==EMPTY){remOpsEmpty++;}
				remOps++;
			}
			ops++;
			gettimeofday(&now,NULL);
		}
	} else if (this->type == AccessPattern::RANDOM) {
		while(now.tv_sec < time_up.tv_sec 
		|| (now.tv_sec==time_up.tv_sec && now.tv_usec<time_up.tv_usec) ){
			r = nextRand(r);
	
			if (r%4==0) {
				q->left_push(ops+1,tid);
				insOps++;
			} else if (r%4==1) {
				if(q->left_pop(tid)==EMPTY){remOpsEmpty++;}
				remOps++;
			} else if (r%4==2) {
				q->right_push(ops+1,tid);
				insOps++;
			} else if (r%4==3) {
				if(q->right_pop(tid)==EMPTY){remOpsEmpty++;}
				remOps++;
			}
			ops++;
			gettimeofday(&now,NULL);
		}
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

void DequeInsertRemoveTest::cleanup(GlobalTestConfig* gtc){
	delete q;
}

void DequeLatencyTest::init(GlobalTestConfig* gtc){
	Rideable* ptr = gtc->allocRideable();
	this->q = dynamic_cast<RDeque*>(ptr);
	if (!q) {
		 errexit("DequeInsertRemoveTest must be run on RDeque type object.");
	}

	pthread_barrier_init(&pthread_barrier, NULL, gtc->task_num);

	std::map<std::string,std::string>::iterator it;
    it = gtc->environment.find("access_type");

    if (it == gtc->environment.end()) {
	    this->type = AccessPattern::QUEUE;
        printf("using default access type: QUEUE\n");
    } else {
    	const char *str = it->second.c_str();
    	if (!strcmp(str, "STACK")) {
    		this->type = AccessPattern::STACK;
    	} else if (!strcmp(str, "QUEUE")) {
    	    this->type = AccessPattern::QUEUE;
		} else if (!strcmp(str, "RANDOM")) {
			this->type = AccessPattern::RANDOM;
		} else {
	        printf("unrecognized access pattern - using default access type: QUEUE\n");
		    this->type = AccessPattern::QUEUE;
    	}
    }

	gtc->recorder->addThreadField("phase1_insOps_total",&Recorder::sumInts);
	gtc->recorder->addThreadField("phase1_insOps_stddev",&Recorder::stdDevInts);
	gtc->recorder->addThreadField("phase1_insOps_each",&Recorder::concat);
	gtc->recorder->addThreadField("phase1_remOps_total",&Recorder::sumInts);
	gtc->recorder->addThreadField("phase1_remOps_stddev",&Recorder::stdDevInts);
	gtc->recorder->addThreadField("phase1_remOps_each",&Recorder::concat);
	gtc->recorder->addThreadField("phase1_remOpsEmpty_total",&Recorder::sumInts);
	gtc->recorder->addThreadField("phase1_remOpsEmpty_stddev",&Recorder::stdDevInts);
	gtc->recorder->addThreadField("phase1_remOpsEmpty_each",&Recorder::concat);

	gtc->recorder->addGlobalField("phase2_insOps_total");
	gtc->recorder->addGlobalField("phase2_remOps_total");
	gtc->recorder->addGlobalField("phase2_remOpsEmpty_total");
}

int DequeLatencyTest::execute(GlobalTestConfig* gtc, LocalTestConfig* ltc){
	int ops = 0;

	innerExec(gtc, ltc, 1);

	pthread_barrier_wait(&pthread_barrier);

	if (ltc->tid == 0) {
		gettimeofday(&gtc->start, NULL);
        gtc->finish=gtc->start;
		gtc->finish.tv_sec+=gtc->interval;

		ops = innerExec(gtc, ltc, 2);		
	}

	return ops;
}

int DequeLatencyTest::innerExec(GlobalTestConfig* gtc, LocalTestConfig* ltc, int phase) {
	struct timeval time_up = gtc->finish;
	struct timeval now;
	gettimeofday(&now,NULL);
	int ops = 0;
	int insOps = 0;
	int remOps = 0;
	int remOpsEmpty = 0;
	unsigned int r = ltc->seed;
	int tid = ltc->tid;

	if (this->type == AccessPattern::QUEUE) {
		while(now.tv_sec < time_up.tv_sec 
			|| (now.tv_sec==time_up.tv_sec && now.tv_usec<time_up.tv_usec) ){
			r = nextRand(r);
		
			if (r%2==0) {
				q->right_push(ops+1,tid);
				insOps++;
			} else {
				if (q->left_pop(tid)==EMPTY) {remOpsEmpty++;}
				remOps++;
			}
			ops++;
			gettimeofday(&now,NULL);
		}
	} else if (this->type == AccessPattern::STACK) {
		while(now.tv_sec < time_up.tv_sec 
		|| (now.tv_sec==time_up.tv_sec && now.tv_usec<time_up.tv_usec) ){
			r = nextRand(r);
	
			if(r%2==0){
				q->right_push(ops+1,tid);
				insOps++;
			} else{
				if(q->right_pop(tid)==EMPTY){remOpsEmpty++;}
				remOps++;
			}
			ops++;
			gettimeofday(&now,NULL);
		}
	} else if (this->type == AccessPattern::RANDOM) {
		while(now.tv_sec < time_up.tv_sec 
		|| (now.tv_sec==time_up.tv_sec && now.tv_usec<time_up.tv_usec) ){
			r = nextRand(r);
	
			if (r%4==0) {
				q->left_push(ops+1,tid);
				insOps++;
			} else if (r%4==1) {
				if(q->left_pop(tid)==EMPTY){remOpsEmpty++;}
				remOps++;
			} else if (r%4==2) {
				q->right_push(ops+1,tid);
				insOps++;
			} else if (r%4==3) {
				if(q->right_pop(tid)==EMPTY){remOpsEmpty++;}
				remOps++;
			}
			ops++;
			gettimeofday(&now,NULL);
		}
	}
	
	if (phase == 1) {
		gtc->recorder->reportThreadInfo("phase1_insOps_total",insOps,ltc->tid);
		gtc->recorder->reportThreadInfo("phase1_insOps_stddev",insOps,ltc->tid);
		gtc->recorder->reportThreadInfo("phase1_insOps_each",insOps,ltc->tid);
		gtc->recorder->reportThreadInfo("phase1_remOps_total",remOps,ltc->tid);
		gtc->recorder->reportThreadInfo("phase1_remOps_stddev",remOps,ltc->tid);
		gtc->recorder->reportThreadInfo("phase1_remOps_each",remOps,ltc->tid);
		gtc->recorder->reportThreadInfo("phase1_remOpsEmpty_total",remOpsEmpty,ltc->tid);
		gtc->recorder->reportThreadInfo("phase1_remOpsEmpty_stddev",remOpsEmpty,ltc->tid);
		gtc->recorder->reportThreadInfo("phase1_remOpsEmpty_each",remOpsEmpty,ltc->tid);
	} else if (phase == 2) {
		gtc->recorder->reportGlobalInfo("phase2_insOps_total",insOps);
		gtc->recorder->reportGlobalInfo("phase2_remOps_total",remOps);
		gtc->recorder->reportGlobalInfo("phase2_remOpsEmpty_total",remOpsEmpty);
	}

	return ops;
}

void DequeLatencyTest::cleanup(GlobalTestConfig* gtc){

}
