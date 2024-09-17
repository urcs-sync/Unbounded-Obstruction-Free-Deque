#include "SGLQueue.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>

SGLQueue::SGLQueue(){
	q = new std::list<int32_t>();
	lk.store(-1,std::memory_order::memory_order_release);
}
SGLQueue::~SGLQueue(){}

int32_t SGLQueue::dequeue(int tid){
	lockAcquire(tid);
	int32_t v=0;
	if(!q->empty()){
		v = q->front();
		q->pop_front();
	}
	else{v=EMPTY;}
	lockRelease(tid);
	return v;
}

void SGLQueue::enqueue(int32_t val,int tid){
	lockAcquire(tid);
	q->push_back(val);
	lockRelease(tid);
}

// Simple test and set lock
/// There are better ways to do this...
void SGLQueue::lockAcquire(int32_t tid){
	int unlk = -1;
	while(!lk.compare_exchange_strong(unlk, tid,std::memory_order::memory_order_acq_rel)){
		unlk = -1; // compare_exchange puts the old value into unlk, so set it back
	}
	assert(lk.load()==tid);
}

void SGLQueue::lockRelease(int32_t tid){
	assert(lk==tid);
	int unlk = -1;
	lk.store(unlk,std::memory_order::memory_order_release);
}


SGLQueue::SGLQueue(std::list<int32_t>* contents){
	lk.store(-1,std::memory_order::memory_order_release);
	q = new std::list<int32_t>();
	q->assign(contents->begin(),contents->end());
}

