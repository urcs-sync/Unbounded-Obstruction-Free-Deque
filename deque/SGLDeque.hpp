
#ifndef SGL_DEQUE_HPP
#define SGL_DEQUE_HPP

#include <deque>
#include <cassert>
#include <cinttypes>

#include "RDeque.hpp"
#include "Rideable.hpp"
#include "ConcurrentPrimitives.hpp"

template<typename T> class SGLDeque : public RDeque {
public:
	/* --- Constructors --- */
	SGLDeque(T empty);
	/* --- Instance Methods (Interface) --- */
	void right_push(T value, int tid);
	void left_push(T value, int tid);
	T right_pop(int tid);
	T left_pop(int tid);
	inline void lock();
	inline void unlock();
private:
	/* --- Instance Fields --- */
	std::deque<T> m_deque;
	T m_empty;
	volatile int m_nLock; 
};

class SGLDequeFactory : public RContainerFactory {
public:
	RContainer *build(GlobalTestConfig *cfg);
};

/* --- Implementation --- */

template<typename T> SGLDeque<T>::SGLDeque(T empty) : 
m_empty(empty),
m_nLock(0) {

}

template<typename T> void SGLDeque<T>::right_push(T value, int tid) {
	lock();
	m_deque.push_back(value);
	unlock();
}

template<typename T> void SGLDeque<T>::left_push(T value, int tid) {
	lock();
	m_deque.push_front(value);	
	unlock();
}

template<typename T> T SGLDeque<T>::right_pop(int tid) {
	T value = m_empty;
	lock();
	if (!m_deque.empty()) {
		value = m_deque.back();
		m_deque.pop_back();
	}
	unlock();
	return value;
}

template<typename T> T SGLDeque<T>::left_pop(int tid) {
	T value = m_empty;
	lock();
	if (!m_deque.empty()) {
		value = m_deque.front();
		m_deque.pop_front();
	}
	unlock();
	return value;
}

template<typename T> void SGLDeque<T>::lock() {
	while (__sync_lock_test_and_set(&m_nLock, 1)) {
		while (m_nLock);
	}
}

template<typename T> void SGLDeque<T>::unlock() {
	__sync_lock_release(&m_nLock);
}

RContainer *SGLDequeFactory::build(GlobalTestConfig *cfg) {
	return new SGLDeque<int32_t>(EMPTY);
}

#endif