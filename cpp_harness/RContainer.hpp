#ifndef RCONTAINER_HPP
#define RCONTAINER_HPP

#include <assert.h>
#include <stddef.h>
#include <atomic>
#include <string>
#include "Rideable.hpp"

#include "ConcurrentPrimitives.hpp"

#ifndef _REENTRANT
#define _REENTRANT		/* basic 3-lines for threads */
#endif


#define OK 1
#define EMPTY 0

class RContainer : public virtual Rideable{
public:
	// dequeues item from queue. Returns EMPTY if empty.
	// tid: Thread id, unique across all threads
	virtual int32_t remove(int tid)=0;

	// enqueues val into queue.
	// tid: Thread id, unique across all threads
	virtual void insert(int32_t val,int tid)=0;

	virtual void addThreadLogs(Recorder *r) { }
	virtual void reportThreadLogs(Recorder *r, int tid) { }
};


class RContainerFactory : public virtual RideableFactory{
public:
	virtual RContainer* build(GlobalTestConfig* gtc)=0;
	virtual ~RContainerFactory(){};
};

class RQueue : public virtual  RContainer{
public:

	// dequeues item from queue. Returns EMPTY if empty.
	// tid: Thread id, unique across all threads
	virtual int32_t dequeue(int tid)=0;

	// enqueues val into queue.
	// tid: Thread id, unique across all threads
	virtual void enqueue(int32_t val,int tid)=0;


	int32_t remove(int tid){return dequeue(tid);}
	void insert(int32_t val,int tid){return enqueue(val,tid);}

};

class RStack : public virtual  RContainer{
public:
	// dequeues item from queue. Returns EMPTY if empty.
	// tid: Thread id, unique across all threads
	virtual int32_t pop(int tid)=0;

	// enqueues val into queue.
	// tid: Thread id, unique across all threads
	virtual void push(int32_t val,int tid)=0;

	int32_t remove(int tid){return pop(tid);}
	void insert(int32_t val,int tid){return push(val,tid);}

};

#endif
