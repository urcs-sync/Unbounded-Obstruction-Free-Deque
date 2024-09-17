#ifndef SGL_QUEUE
#define SGL_QUEUE

#ifndef _REENTRANT
#define _REENTRANT
#endif

#include <queue>
#include <list>
#include <string>
#include <atomic>
#include "RContainer.hpp"

class SGLQueue : public RQueue{

private:
	void lockAcquire(int32_t tid);
	void lockRelease(int32_t tid);

	std::list<int32_t>* q=NULL;
	std::atomic<int32_t> lk;


public:
	SGLQueue();
	~SGLQueue();
	SGLQueue(std::list<int32_t>* contents);

	int32_t dequeue(int tid);
	void enqueue(int32_t val,int tid);
	
};

class SGLQueueFactory : public RContainerFactory{
	SGLQueue* build(GlobalTestConfig* gtc){
		return new SGLQueue();
	}
};

#endif
