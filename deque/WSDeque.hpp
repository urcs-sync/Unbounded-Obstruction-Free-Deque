
#ifndef WSDEQUE_HPP
#define WSDEQUE_HPP

/* 
 * Implementation of the dynamic circular work-stealing deque 
 * (David Chase and Yossi Lev)
 */

#include <atomic>
#include <cstdlib>
#include <cinttypes>
#include "Rideable.hpp"
#include "BlockPool.hpp"
#include "RContainer.hpp"
#include "RAllocator.hpp"
#include "HazardTracker.hpp"
#include "ConcurrentPrimitives.hpp"

template<typename T> class WSDeque : public RQueue {
private:
	/* --- Inner Types --- */
	enum OP_FLAG { OP_SUCCESS = 0, OP_EMPTY, OP_ABORT };	
public:
	/* --- Constructors & Destructor --- */
	WSDeque(int numThreads, bool glibc, T empty) :
	m_empty(empty), m_nThreadCount(numThreads), m_haz(numThreads, &m_alloc, 1, 1) {
		m_pDeques = new deque_t*[m_nThreadCount];
		for (int i = 0; i < m_nThreadCount; ++i) {
			m_pDeques[i] = new deque_t(10, m_haz); // intial ring size (1 << 10)
		}
	}
	~WSDeque() {
		for (int i = 0; i < m_nThreadCount; ++i)
			delete m_pDeques[i];
		delete[] m_pDeques;
	}
	/* --- Instance Methods (Interface) --- */
	void enqueue(T t, int tid) {
		deque_t *d = m_pDeques[tid];
		d->push(t, tid);
	}
	T dequeue(int tid) {
		T out;
		deque_t *d = m_pDeques[tid];
		OP_FLAG flag = d->pop(out, tid);
		if (flag == OP_SUCCESS) {
			return out;
		} else {
			// randomly select another deque to try to steal from.
			int r;
			for (int i = 0; i < 10; ++i) {
				r = rand_r(&st_nSeed) % m_nThreadCount;
				if (r == tid)
					continue;
				d = m_pDeques[r];
				flag = d->steal(out, tid);
				if (flag == OP_SUCCESS) {
					return out;
				}
			}
		}
		return m_empty;
	}
private:
	/* --- Inner Types --- */
	struct ring_t {
	private:
		T *arr;
		long logsize;
	public:
		ring_t(long logsize) : logsize(logsize) {
			arr = new T[1 << logsize];
		}
		~ring_t() {
			delete[] arr;
		}
		inline long size() { return 1 << logsize; }
		inline T get(long i) { return arr[i % size()]; }
		inline void put(long i, T val) { arr[i % size()] = val; }
		inline ring_t *grow(long b, long t) {
			ring_t *a = new ring_t(logsize + 1);
			for (long i = t; i < b; ++i)
				a->put(i, get(i));
			return a;
		}
	};
	struct deque_t {
	public:
		/* --- Constructors & Destructor --- */
		deque_t(long logsize, HazardTracker& haz) : 
		m_haz(haz),
		m_nTop(0), 
		m_nBottom(0), 
		m_pActiveArray(new ring_t(logsize)) { 

		}
		/* --- Instance Methods (Interface) --- */
		void push(T& o, int tid) {
			long b = getBottom();
			long t = getTop();
			ring_t *ring = getActiveArray();
			long size = b - t;
			if (size >= ring->size() - 1) {
				ring_t *old = ring;
				ring = ring->grow(b, t);
				setActiveArray(ring);
				m_haz.retire(old, tid);
			}
			ring->put(b, o);
			setBottom(b + 1);
		}
		OP_FLAG pop(T& out, int tid) {
			long b = getBottom();
			ring_t *ring = getActiveArray();
			b = b - 1;
			setBottom(b);
			long t = getTop();
			long size = b - t;
			if (size < 0) {
				setBottom(t);
				return OP_EMPTY;
			}
			T o = ring->get(b);
			if (size > 0) {
				out = o;
				return OP_SUCCESS;
			}
			if (casTop(t, t + 1) == false)
				return OP_EMPTY;
			setBottom(t + 1);
			out = o;
			return OP_SUCCESS;
		}
		OP_FLAG steal(T& out, int tid) {
			OP_FLAG flag = OP_SUCCESS;
			long t, b;
			ring_t *ring;
			T o;

			repeat:
			t = getTop();
			b = getBottom();
			ring = getActiveArray();

			m_haz.reserve(ring, 0, tid);
			if (ring != getActiveArray()) 
				goto repeat;

			long size = b - t;
			if (size <= 0) {
				flag = OP_EMPTY;
				goto ret;
			}

			o = ring->get(t);
			if (casTop(t, t + 1) == false) {
				flag = OP_ABORT;
				goto ret;
			}
			out = o;

			ret:
			m_haz.clearAll(tid);
			return flag;
		}
	private:
		/* --- Instance Methods (Helper) --- */
		inline bool casTop(long e, long t) {
			return m_nTop.ui.compare_exchange_weak(e, t, std::memory_order_acq_rel);
		}
		inline void setTop(long t, std::memory_order ord = std::memory_order_release) {
			m_nTop.ui.store(t, ord);
		}
		inline long getTop(std::memory_order ord = std::memory_order_acquire) {
			return m_nTop.ui.load(ord);
		}
		inline long getBottom(std::memory_order ord = std::memory_order_relaxed) {
			return m_nBottom.ui.load(ord);
		}
		inline void setBottom(long b, std::memory_order ord = std::memory_order_relaxed) {
			m_nBottom.ui.store(b, ord);
		}
		inline ring_t *getActiveArray(std::memory_order ord = std::memory_order_acquire) {
			return m_pActiveArray.ui.load(ord);
		}
		inline void setActiveArray(ring_t *a, std::memory_order ord = std::memory_order_release) {
			m_pActiveArray.ui.store(a, ord);
		}
		/* --- Instance Fields --- */
		HazardTracker& m_haz;
		paddedAtomic<long> m_nBottom;
		paddedAtomic<long> m_nTop;
		paddedAtomic<ring_t*> m_pActiveArray;
	};
	class STDAlloc : public RAllocator {
		void* allocBlock(int tid) {
			// unused...
			return NULL;
		}
		void freeBlock(void* ptr, int tid) {
			delete (ring_t*)ptr;
		}
	};
	/* --- Static Fields --- */
	static __thread unsigned st_nSeed;
	/* --- Instance Fields --- */
	HazardTracker m_haz;
	STDAlloc m_alloc;
	T m_empty;
	deque_t **m_pDeques;
	int m_nThreadCount;
};

class WSDequeFactory : public RContainerFactory {
public:
	RContainer *build(GlobalTestConfig *gtc) {
		return new WSDeque<int32_t>(gtc->task_num, gtc->environment["glibc"] == "1", EMPTY);
	}
};

/* ---------------------- */
/* --- Implementation --- */
/* ---------------------- */

template<typename T> __thread unsigned WSDeque<T>::st_nSeed = 0;

#endif 