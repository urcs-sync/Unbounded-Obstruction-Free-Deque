#ifndef TESTS_HPP
#define TESTS_HPP

#ifndef _REENTRANT
#define _REENTRANT		/* basic 3-lines for threads */
#endif

#include <atomic>
#include "Harness.hpp"
#include "RDeque.hpp"

class PotatoTest : public Test{
private:
	UIDGenerator* ug;
	int hotPotatoPenalty=0;
	inline int executeQueue(GlobalTestConfig* gtc, LocalTestConfig* ltc);

public:
	PotatoTest(int delay){hotPotatoPenalty = delay;}
	PotatoTest(){}
	RContainer* q;
	void init(GlobalTestConfig* gtc);
	int execute(GlobalTestConfig* gtc, LocalTestConfig* ltc);
	void cleanup(GlobalTestConfig* gtc);
};


class QueueVerificationTest : public Test{
public:
	RDeque* q;
	std::atomic<bool> passed;
	UIDGenerator* ug;

	void init(GlobalTestConfig* gtc);
	int execute(GlobalTestConfig* gtc, LocalTestConfig* ltc);
	void cleanup(GlobalTestConfig* gtc);
};

class StackVerificationTest : public Test{
public:
	RDeque* q;
	std::atomic<bool> passed;
	std::atomic<bool> done;
	UIDGenerator* ug;
	pthread_barrier_t pthread_barrier;
	int opsPerPhase;
	std::atomic<int> phaseCount;


	void barrier();
	void init(GlobalTestConfig* gtc);
	int execute(GlobalTestConfig* gtc, LocalTestConfig* ltc);
	void cleanup(GlobalTestConfig* gtc){}
};

class DequeInsertRemoveTest : public Test{
public:
	enum AccessPattern { QUEUE, STACK, RANDOM };

	RDeque* q;
	AccessPattern type;

	void init(GlobalTestConfig* gtc);
	int execute(GlobalTestConfig* gtc, LocalTestConfig* ltc);
	void cleanup(GlobalTestConfig* gtc);
};

class DequeLatencyTest : public Test {
public:
	void init(GlobalTestConfig* gtc);
	int execute(GlobalTestConfig* gtc, LocalTestConfig* ltc);
	void cleanup(GlobalTestConfig* gtc);
private:
	int innerExec(GlobalTestConfig* gtc, LocalTestConfig* ltc, int phase);

	enum AccessPattern { QUEUE, STACK, RANDOM };

	RDeque* q;
	AccessPattern type;
	pthread_barrier_t pthread_barrier;
};

#endif
