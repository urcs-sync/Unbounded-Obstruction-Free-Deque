
#ifndef MMDEQUE_HPP
#define MMDEQUE_HPP

#include <atomic>
#include <cassert>
#include <cinttypes>

#include "Rideable.hpp"
#include "BlockPool.hpp"
#include "RDeque.hpp"
#include "HazardTracker.hpp"
#include "ConcurrentPrimitives.hpp"

/*
 * Implementation of Maged Michael's lock-free deque
 */

template<typename T> class MMDeque : public RDeque {

private:

	/* --- Inner Types --- */

	enum StatusType { STABLE = 0, RPUSH = 1, LPUSH = 2 };
	
	struct node_t {
		cptr<node_t> left, right;
		T data;
	};

	struct anchor_t {
	private:
			node_t *m_leftMost, *m_rightMost;
	public:
		inline void set(node_t *left, node_t *right, StatusType status) {
			m_leftMost = left;
			m_rightMost = right;
			setStatus(status);
		}
		inline void setStatus(StatusType s) {
			assert(s >= 0 && s <= 2);
			int k = (int)m_leftMost;
			k &= 0xFFFFFFFC;
			k |= s;
			m_leftMost = (node_t*)k; 
		}
		inline StatusType getStatus() const {
			int k = (int)m_leftMost;
			return (StatusType)(k & 0x3);
		}
		inline node_t *getLeft() const {
			int k = (int)m_leftMost;
			k &= 0xFFFFFFFC;
			return (node_t*)k;
		}
		inline node_t *getRight() const {
			return m_rightMost;
		}
		inline bool operator!=(const anchor_t& o) const {
			return (m_leftMost != o.m_leftMost) || (m_rightMost != o.m_rightMost);
		}
	};

public:

	/* --- Constructors & Destructors --- */

	MMDeque(int threadCount, bool glibc, const T empty);
	~MMDeque();

	/* ---- Instance Methods (Inteface) --- */

	T left_pop(int tid);
	T right_pop(int tid);
	void left_push(T t, int tid);
	void right_push(T t, int tid);

	bool is_empty(const T& data);

private:	

	/* --- Instance Methods (Helper) --- */

	anchor_t getAnchor(std::memory_order ord = std::memory_order_acquire);
	bool casAnchor(anchor_t exp, anchor_t a, std::memory_order ord = std::memory_order_release);
	bool casAnchor(anchor_t exp, node_t *left, node_t *right, StatusType status, std::memory_order ord = std::memory_order_release);

	void stabilize(const anchor_t& a, int tid);
	void stabilizeLeft(const anchor_t& a, int tid);
	void stabilizeRight(const anchor_t& a, int tid);

	/* --- Instance Fields --- */

	HazardTracker m_haz;
	BlockPool<node_t> m_nodePool;
	std::atomic<anchor_t> m_anchor;

	const T m_empty;	

};

class MMDequeFactory : public RContainerFactory {
public:
	RContainer *build(GlobalTestConfig *gtc) {
		return new MMDeque<int32_t>(gtc->task_num, gtc->environment["glibc"] == "1", EMPTY);
	}
};


/* --- Implementation --- */


/* --- Constructors & Destructors --- */

template<typename T> MMDeque<T>::MMDeque(int threadCount, bool glibc, T empty) :
m_nodePool(threadCount, glibc), 
m_empty(empty), 
m_haz(threadCount, &m_nodePool, 3, 3) {
	MMDeque<T>::anchor_t a;
	a.set(NULL, NULL, MMDeque<T>::STABLE);
	m_anchor.store(a);
}

template<typename T> MMDeque<T>::~MMDeque() {
  // @todo Clean up allocations
}

/* --- Instance Methods (Interface) --- */

template<typename T> bool MMDeque<T>::is_empty(const T& data) {
	return (data == m_empty);
} 

template<typename T> void MMDeque<T>::right_push(T t, int tid) {
	MMDeque<T>::node_t *node = m_nodePool.alloc(tid);
	node->data = t;
	for (;;) {
		anchor_t a = getAnchor();
		if (a.getRight() == NULL) {
			if (casAnchor(a, node, node, a.getStatus()))
				return;
		} else if (a.getStatus() == MMDeque<T>::STABLE) {
			node->left.init(a.getRight(), 0);
			
			MMDeque<T>::anchor_t a2;
			a2.set(a.getLeft(), node, MMDeque<T>::RPUSH);

			if (casAnchor(a, a2)) {
				stabilizeRight(a2, tid);
				return;
			}
		} else {
			stabilize(a, tid);
		}
	}
}

template<typename T> void MMDeque<T>::left_push(T t, int tid) {
	MMDeque<T>::node_t *node = m_nodePool.alloc(tid);
	node->data = t;
	for (;;) {
		anchor_t a = getAnchor();
		if (a.getLeft() == NULL) {
			if (casAnchor(a, node, node, a.getStatus()))
				return;
		} else if (a.getStatus() == MMDeque<T>::STABLE) {
			node->right.init(a.getLeft(), 0);
			
			MMDeque<T>::anchor_t a2;
			a2.set(node, a.getRight(), MMDeque<T>::LPUSH);
				
			if (casAnchor(a, a2)) {
				stabilizeLeft(a2, tid);
				return;
			}
		} else {
			stabilize(a, tid);
		}
	}
}

template<typename T> T MMDeque<T>::right_pop(int tid) {
	MMDeque<T>::anchor_t a;
	for (;;) {
		a = getAnchor();
		if (a.getRight() == NULL)
			return m_empty;
		if (a.getRight() == a.getLeft()) {
			if (casAnchor(a, NULL, NULL, a.getStatus()))
				break;
		} else if (a.getStatus() == MMDeque<T>::STABLE) {
			m_haz.reserve(a.getLeft(), 0, tid);
			m_haz.reserve(a.getRight(), 1, tid);
			if (a != getAnchor())
				continue;
			node_t *prev = a.getRight()->left.ptr();
			if (casAnchor(a, a.getLeft(), prev, a.getStatus()))
				break;
		} else {
			stabilize(a, tid);
		}
	}
	T data = a.getRight()->data;
	m_haz.retire(a.getRight(), tid);
	m_haz.clearAll(tid);
	return data;
}

template<typename T> T MMDeque<T>::left_pop(int tid) {
	MMDeque<T>::anchor_t a;
	for (;;) {
		a = getAnchor();
		if (a.getLeft() == NULL)
			return m_empty;
		if (a.getLeft() == a.getRight()) {
			if (casAnchor(a, NULL, NULL, a.getStatus()))
				break;
		} else if (a.getStatus() == MMDeque<T>::STABLE) {
			m_haz.reserve(a.getLeft(), 0, tid);
			m_haz.reserve(a.getRight(), 1, tid);
			if (a != getAnchor())
				continue;
			node_t *prev = a.getLeft()->right.ptr();
			if (casAnchor(a, prev, a.getRight(), a.getStatus()))
				break;
		} else {
			stabilize(a, tid);
		}
	}
	T data = a.getLeft()->data;
	m_haz.retire(a.getLeft(), tid);
	m_haz.clearAll(tid);
	return data;
}


/* --- Instance Methods (Helper) --- */

template<typename T> typename MMDeque<T>::anchor_t MMDeque<T>::getAnchor(std::memory_order ord/* = std::memory_order_acquire*/) {
	return m_anchor.load(ord);
}

template<typename T> bool MMDeque<T>::casAnchor(anchor_t exp, anchor_t a, std::memory_order ord/* = std::memory_order_release*/) {
	return m_anchor.compare_exchange_weak(exp, a, ord, std::memory_order_acquire);
}

template<typename T> bool MMDeque<T>::casAnchor(anchor_t exp, node_t *left, node_t *right, StatusType status, std::memory_order ord/* = std::memory_order_release*/) {
	MMDeque<T>::anchor_t a2;
	a2.set(left, right, status);
	return m_anchor.compare_exchange_weak(exp, a2, ord, std::memory_order_acquire);
}

template<typename T> void MMDeque<T>::stabilize(const anchor_t& a, int tid) {
	if (a.getStatus() == MMDeque<T>::RPUSH) {
		stabilizeRight(a, tid);
	} else if (a.getStatus() == MMDeque<T>::LPUSH) {
		stabilizeLeft(a, tid);
	}
}

template<typename T> void MMDeque<T>::stabilizeRight(const anchor_t& a, int tid) {
	m_haz.reserve(a.getLeft(), 0, tid);
	m_haz.reserve(a.getRight(), 1, tid);
	if (a != getAnchor())
		return;

	MMDeque<T>::node_t *prev = a.getRight()->left.ptr();
	m_haz.reserve(prev, 2, tid);
	if (a != getAnchor())
		return;

	cptr_local<MMDeque<T>::node_t> prevNext(prev->right);
	if (prevNext.ptr() != a.getRight()) {
		if (a != getAnchor())
			return;
		if (!prev->right.CAS(prevNext, a.getRight()))
			return;
	}

	casAnchor(a, a.getLeft(), a.getRight(), MMDeque<T>::STABLE);

	m_haz.clearAll(tid);
}

template<typename T> void MMDeque<T>::stabilizeLeft(const anchor_t& a, int tid) {
	m_haz.reserve(a.getLeft(), 0, tid);
	m_haz.reserve(a.getRight(), 1, tid);
	if (a != getAnchor())
		return;

	MMDeque<T>::node_t *prev = a.getLeft()->right.ptr();
	m_haz.reserve(prev, 2, tid);
	if (a != getAnchor())
		return;

	cptr_local<MMDeque<T>::node_t> prevNext(prev->left);
	if (prevNext.ptr() != a.getLeft()) {
		if (a != getAnchor())
			return;
		if (!prev->left.CAS(prevNext, a.getLeft()))
			return;
	}

	casAnchor(a, a.getLeft(), a.getRight(), MMDeque<T>::STABLE);

	m_haz.clearAll(tid);
}

#endif