
#ifndef RDEQUE_HPP
#define RDEQUE_HPP

#include <atomic>
#include "RContainer.hpp"

class RDeque : public virtual RContainer {
public:
	virtual ~RDeque() { };

	// left pop from queue. Returns EMPTY if empty.
	// tid: Thread id, unique across all threads
	virtual int32_t left_pop(int tid)=0;

	// left push val into queue.
	// tid: Thread id, unique across all threads
	virtual void left_push(int32_t val,int tid)=0;

	// right pop from queue. Returns EMPTY if empty.
	// tid: Thread id, unique across all threads
	virtual int32_t right_pop(int tid)=0;

	// right push val into queue.
	// tid: Thread id, unique across all threads
	virtual void right_push(int32_t val,int tid)=0;

	int32_t remove(int tid){return left_pop(tid);}
	void insert(int32_t val,int tid){return left_push(val,tid);}

};

#endif
