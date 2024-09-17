#ifndef HAZARD_TRACKER_HPP
#define HAZARD_TRACKER_HPP

#ifndef _REENTRANT
#define _REENTRANT
#endif

#include <queue>
#include <list>
#include <vector>
#include <atomic>
#include "ConcurrentPrimitives.hpp"
#include "RAllocator.hpp"

class HazardTracker{
private:
	int task_num;
	int slotsPerThread;
	int freq;
	bool collect;

	RAllocator* mem;

	paddedAtomic<void*>* slots;
	padded<int>* cntrs;
	padded<std::list<void*>>* retired; // @todo use different structure to prevent malloc locking....

public:
//	~HazardTracker();
	HazardTracker(int task_num, RAllocator* mem, int slotsPerThread, int emptyFreq, bool collect);
	HazardTracker(int task_num, RAllocator* mem, int slotsPerThread, int emptyFreq);

	void reserve(void* ptr, int slot, int tid);
	void clearSlot(int slot, int tid);
	void clearAll(int tid);

	void retire(void* ptr, int tid);
	void empty(int tid);
	
};


#endif
