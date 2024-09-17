#include "HazardTracker.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <algorithm>   

using namespace std;


HazardTracker::HazardTracker(int task_num, RAllocator* mem, int slotsPerThread, int emptyFreq, bool collect){
	this->task_num = task_num;
	this->slotsPerThread = slotsPerThread;
	this->freq = emptyFreq;
	this->mem = mem;
	slots = new paddedAtomic<void*>[task_num*slotsPerThread];
	for (int i = 0; i<task_num*slotsPerThread; i++){
		slots[i]=NULL;
	}
	retired = new padded<list<void*>>[task_num];
	cntrs = new padded<int>[task_num];
	for (int i = 0; i<task_num; i++){
		cntrs[i]=0;
		retired[i].ui = list<void*>();
	}
	this->collect = collect;
}

HazardTracker::HazardTracker(int task_num, RAllocator* mem, int slotsPerThread, int emptyFreq){
	this->task_num = task_num;
	this->slotsPerThread = slotsPerThread;
	this->freq = emptyFreq;
	this->mem = mem;
	slots = new paddedAtomic<void*>[task_num*slotsPerThread];
	for (int i = 0; i<task_num*slotsPerThread; i++){
		slots[i]=NULL;
	}
	retired = new padded<list<void*>>[task_num];
	cntrs = new padded<int>[task_num];
	for (int i = 0; i<task_num; i++){
		retired[i].ui = list<void*>();
		cntrs[i]=0;
	}
	this->collect = true;
}

void HazardTracker::reserve(void* ptr, int slot, int tid){
	slots[tid*slotsPerThread+slot] = ptr;
}
void HazardTracker::clearSlot(int slot, int tid){
	slots[tid*slotsPerThread+slot] = NULL;
}
void HazardTracker::clearAll(int tid){
	for(int i = 0; i<slotsPerThread; i++){
		slots[tid*slotsPerThread+i] = NULL;
	}
}

void HazardTracker::retire(void* ptr, int tid){
	if(ptr==NULL){return;}
	list<void*>* myTrash = &(retired[tid].ui);
	assert(find(myTrash->begin(), myTrash->end(), ptr)==myTrash->end());  
	myTrash->push_back(ptr);	
	if(collect && cntrs[tid]==freq){
		cntrs[tid]=0;
		empty(tid);
	}
	cntrs[tid].ui++;
}

void HazardTracker::empty(int tid){
	list<void*>* myTrash = &(retired[tid].ui);
	for (std::list<void*>::iterator iterator = myTrash->begin(), end = myTrash->end(); iterator != end; ++iterator) {
		bool danger = false;
		void* ptr = *iterator;
		for (int i = 0; i<task_num*slotsPerThread; i++){
			if(ptr == slots[i].ui){
				danger = true;
				break;
			}
		}
		if(!danger){
			mem->freeBlock(ptr,tid);
			iterator = myTrash->erase(iterator);
		}
	}

	return;
}

