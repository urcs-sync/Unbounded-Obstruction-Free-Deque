#ifndef RALLOCATOR_HPP
#define RALLOCATOR_HPP

#include <string>
#include "Rideable.hpp"

#ifndef _REENTRANT
#define _REENTRANT		/* basic 3-lines for threads */
#endif

class RAllocator : public Rideable{
public:
	// dequeues item from queue. Returns EMPTY if empty.
	// tid: Thread id, unique across all threads
	virtual void* allocBlock(int tid)=0;

	// enqueues val into queue.
	// tid: Thread id, unique across all threads
	virtual void freeBlock(void* ptr,int tid)=0;
};
#endif
