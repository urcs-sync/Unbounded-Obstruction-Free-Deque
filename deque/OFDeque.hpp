#ifndef OFDEQUE_HPP
#define OFDEQUE_HPP

#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <cassert>
#include <cinttypes> 

#include "RDeque.hpp"
#include "ElimTable.hpp"
#include "BlockPool.hpp"
#include "HazardTracker.hpp"
#include "ConcurrentPrimitives.hpp"

namespace OFDequeTypes {
	enum Type {
		TYPE_LEFT = 0,
		TYPE_RIGHT = 1,
		TYPE_SEALED = 2,
		TYPE_VALUE = 3
	};

	enum Side {
		SIDE_LEFT = 0,
		SIDE_RIGHT = 1
	};
};

template<OFDequeTypes::Side S, typename T, int BufferSize, bool Elimination> struct OFDequeUtils;

template<typename T, int BufferSize, bool Elimination=true> class OFDeque : public RDeque {
public:
	static_assert(sizeof(T) <= 4, "template argument T is larger than 4 bytes");
	/* --- Constructors & Destructor --- */
	OFDeque(T empty, int threadCount, bool glibc);
	~OFDeque();
	/* --- Instance Methods (Interface) --- */
	void left_push(T value, int tid);
	void right_push(T value, int tid);
	T left_pop(int tid);
	T right_pop(int tid);
private:
	/* --- Inner Types --- */
	struct Buffer;
	struct Slot {
		/* --- Instance Methods (Interface) --- */
		inline bool operator==(const Slot &n) { return *((uint64_t*)this) == *((uint64_t*)&n); }

		/* --- Instance Fields --- */
		union {
			T m_value;
			Buffer *m_pLink;
		};

		struct {
			uint32_t m_count : 30;
			uint32_t m_type : 2;
		};
	};
	struct Buffer {
		/* --- Instance Methods (Interface) --- */
		void fill(int split);
		int isSealed();

		OFDequeTypes::Type loadType(int index, std::memory_order order = std::memory_order_relaxed) {
			return (OFDequeTypes::Type)loadSlot(index, order).m_type;
		}

		Slot loadSlot(int index, std::memory_order order = std::memory_order_relaxed) {
			return m_pSlots[index].load(order);
		}

		bool casSafe(int index, Slot exp) { 
			Slot s;
			s.m_type = exp.m_type;
			s.m_value = exp.m_value;
			s.m_count = exp.m_count + 1;
			return m_pSlots[index].compare_exchange_strong(exp, s, std::memory_order_acq_rel, std::memory_order_acquire);
		}

		bool casType(int index, Slot exp, OFDequeTypes::Type type) {
			Slot s;
			s.m_type = type;
			s.m_count = exp.m_count + 1;
			return m_pSlots[index].compare_exchange_strong(exp, s, std::memory_order_acq_rel, std::memory_order_acquire);
		}

		bool casValue(int index, Slot exp, const T &value) {
			Slot s;
			s.m_type = OFDequeTypes::TYPE_VALUE;
			s.m_value = value;
			s.m_count = exp.m_count + 1;
			return m_pSlots[index].compare_exchange_strong(exp, s, std::memory_order_acq_rel, std::memory_order_acquire);
		}

		bool casLink(int index, Slot exp, Buffer *link) {
			Slot s;
			s.m_type = OFDequeTypes::TYPE_VALUE;
			s.m_pLink = link;
			s.m_count = exp.m_count + 1;
			return m_pSlots[index].compare_exchange_strong(exp, s, std::memory_order_acq_rel, std::memory_order_acquire);
		}

		/* --- Instance Fields --- */
		paddedAtomic<int> m_leftLocalHint __attribute__ ((aligned(CACHE_LINE_SIZE)));
		paddedAtomic<int> m_rightLocalHint __attribute__ ((aligned(CACHE_LINE_SIZE)));
		std::atomic<Slot> m_pSlots[BufferSize];
	};

	struct GlobalHint {
		/* --- Constructors --- */
		GlobalHint() noexcept { }
		GlobalHint(Buffer *b, uint32_t u) :
			m_pBuffer(b), m_count(u) { }
		/* --- Instance Fields --- */
		Buffer* m_pBuffer;
		uint32_t m_count;
	};

	struct Edge {
		/* --- Constructors --- */
		Edge() { }
		Edge(Buffer *buffer, int index) : 
			m_pBuffer(buffer), m_index(index) {
		}
		/* --- Instance Fields --- */
		Buffer *m_pBuffer;
		int m_index;
	};

	struct OracleResult {
		/* --- Instance Fields --- */
		GlobalHint m_hint;
		Edge m_edge;
	};

	struct ThreadLog {
		int m_stdPops, m_elimPops;
		int m_stdPushes, m_elimPushes;
		int m_oracleLoops;
		int m_oracleInvokes;
	};

	/* --- Instance Methods (Auxiliary) --- */

	void retire(Buffer *buffer, int tid);
	template<OFDequeTypes::Side S> T doPop(int tid);
	template<OFDequeTypes::Side S> void doPush(const T &value, int tid);
	template<OFDequeTypes::Side S> bool findEdge(Edge &outEdge, GlobalHint hint, int tid);
	
	template<OFDequeTypes::Side S> bool findActiveBuffer(Buffer **outBuffer, GlobalHint hint, int tid);

	template<OFDequeTypes::Side S> OracleResult oracle(int tid);
	template<OFDequeTypes::Side S> GlobalHint reserveHint(int slot, int tid);
	template<OFDequeTypes::Side S> void updateHint(int tid);
	template<OFDequeTypes::Side S> std::atomic<GlobalHint> &getGlobalHint();
	template<OFDequeTypes::Side S> padded<Buffer*> *getBufferCache();
	template<OFDequeTypes::Side S> ElimTable<T> *getElimTable();

	/* --- Static Methods (Auxiliary) --- */

	template<OFDequeTypes::Side S> static constexpr int GetFarLinkIndex();
	template<OFDequeTypes::Side S> static constexpr int GetNearLinkIndex();
	template<OFDequeTypes::Side S> static constexpr int GetFarValueIndex();
	template<OFDequeTypes::Side S> static constexpr int GetNearValueIndex();
	template<OFDequeTypes::Side S> static constexpr int GetFarDirection();
	template<OFDequeTypes::Side S> std::atomic<int> &GetLocalHint(Buffer *buf);
	template<OFDequeTypes::Side S> static constexpr OFDequeTypes::Type GetFarType();
	template<OFDequeTypes::Side S> static constexpr OFDequeTypes::Type GetNearType();

	/* --- Instance Fields --- */

	paddedAtomic<GlobalHint> m_leftGlobalHint __attribute__ ((aligned(CACHE_LINE_SIZE))); 
	paddedAtomic<GlobalHint> m_rightGlobalHint __attribute__ ((aligned(CACHE_LINE_SIZE)));
	
	padded<Buffer*> *m_pLeftBufferCache;
	padded<Buffer*> *m_pRightBufferCache;
	
	BlockPool<Buffer> *m_pBlockPool;
	HazardTracker *m_pHazTracker;
	
	ElimTable<T> *m_pLeftElimTable;
	ElimTable<T> *m_pRightElimTable;
	
	padded<ThreadLog> *m_pThreadLogs;

	const T m_empty;
	const int m_threadCount;
	const int m_scanCountStart;

	/* --- Friends --- */

	friend struct OFDequeUtils<OFDequeTypes::SIDE_LEFT, T, BufferSize, Elimination>;
	friend struct OFDequeUtils<OFDequeTypes::SIDE_RIGHT, T, BufferSize, Elimination>;
};

template<OFDequeTypes::Side S, typename T, int BufferSize, bool Elimination> struct OFDequeUtils {};
template<typename T, int BufferSize, bool Elimination> struct OFDequeUtils<OFDequeTypes::Side::SIDE_LEFT, T, BufferSize, Elimination> {
	static constexpr int GetFarLinkIndex() { return 0; }
	static constexpr int GetNearLinkIndex() { return BufferSize - 1; }
	static constexpr int GetFarValueIndex() { return 1; }
	static constexpr int GetNearValueIndex() { return BufferSize - 2; }

	static constexpr int GetFarDirection() { return -1; }
	
	static constexpr OFDequeTypes::Type GetFarType() { return OFDequeTypes::TYPE_LEFT; }
	static constexpr OFDequeTypes::Type GetNearType() { return OFDequeTypes::TYPE_RIGHT; }

	static std::atomic<typename OFDeque<T, BufferSize, Elimination>::GlobalHint> &GetGlobalHint(OFDeque<T, BufferSize, Elimination> *d) { return d->m_leftGlobalHint.ui; }
	static std::atomic<int> &GetLocalHint(typename OFDeque<T, BufferSize, Elimination>::Buffer *buf) { return buf->m_leftLocalHint.ui; }
	static padded<typename OFDeque<T, BufferSize, Elimination>::Buffer*> *GetBufferCache(OFDeque<T, BufferSize, Elimination> *d) { return d->m_pLeftBufferCache; }

	static ElimTable<T> *GetElimTable(OFDeque<T, BufferSize, Elimination> *d) { return d->m_pLeftElimTable; }
};

template<typename T, int BufferSize, bool Elimination> struct OFDequeUtils<OFDequeTypes::Side::SIDE_RIGHT, T, BufferSize, Elimination> {
	static constexpr int GetFarLinkIndex() { return BufferSize - 1; }
	static constexpr int GetNearLinkIndex() { return 0; }
	static constexpr int GetFarValueIndex() { return BufferSize - 2; }
	static constexpr int GetNearValueIndex() { return 1; }

	static constexpr int GetFarDirection() { return 1; }

	static constexpr OFDequeTypes::Type GetFarType() { return OFDequeTypes::TYPE_RIGHT; }
	static constexpr OFDequeTypes::Type GetNearType() { return OFDequeTypes::TYPE_LEFT; }

	static std::atomic<typename OFDeque<T, BufferSize, Elimination>::GlobalHint> &GetGlobalHint(OFDeque<T, BufferSize, Elimination> *d) { return d->m_rightGlobalHint.ui; }
	static std::atomic<int> &GetLocalHint(typename OFDeque<T, BufferSize, Elimination>::Buffer *buf) { return buf->m_rightLocalHint.ui; }
	static padded<typename OFDeque<T, BufferSize, Elimination>::Buffer*> *GetBufferCache(OFDeque<T, BufferSize, Elimination> *d) { return d->m_pRightBufferCache; }

	static ElimTable<T> *GetElimTable(OFDeque<T, BufferSize, Elimination> *d) { return d->m_pRightElimTable; }
};

template<int BufferSize, bool Elimination> class OFDequeFactory : public RContainerFactory {
	OFDeque<int32_t, BufferSize, Elimination>* build(GlobalTestConfig* gtc){
		return new OFDeque<int32_t, BufferSize, Elimination>(0, gtc->task_num, gtc->environment["glibc"]=="1");
	}
};

template<typename T, int BufferSize, bool Elimination>
OFDeque<T, BufferSize, Elimination>::OFDeque(T empty, int threadCount, bool glibc) :
	m_empty(empty),
	m_threadCount(threadCount),
	m_scanCountStart(threadCount) {

	m_pBlockPool = new BlockPool<Buffer>(threadCount, glibc);

	void *haz = memalign(CACHE_LINE_SIZE, sizeof(HazardTracker));
	m_pHazTracker = new (haz) HazardTracker(threadCount, m_pBlockPool, 2, 2);

	/* allocate left buffer cache */
	m_pLeftBufferCache = (padded<Buffer*>*)memalign(CACHE_LINE_SIZE, sizeof(padded<Buffer*>) * threadCount);
	for (int i = 0; i < threadCount; ++i) {
		m_pLeftBufferCache[i].ui = NULL;
	}

	/* allocate right buffer cache */
	m_pRightBufferCache = (padded<Buffer*>*)memalign(CACHE_LINE_SIZE, sizeof(padded<Buffer*>) * threadCount);
	for (int i = 0; i < threadCount; ++i) {
		m_pRightBufferCache[i].ui = NULL;
	}

	/* allocate elimination tables */
	void *elimTable;

	elimTable = memalign(CACHE_LINE_SIZE, sizeof(ElimTable<T>));
	m_pLeftElimTable = new (elimTable) ElimTable<T>(threadCount);
	
	elimTable = memalign(CACHE_LINE_SIZE, sizeof(ElimTable<T>));
	m_pRightElimTable = new (elimTable) ElimTable<T>(threadCount);

	/* allocate initial buffer */
	Buffer *buffer = m_pBlockPool->alloc(0);

	/* fill buffer (this will set local hint as well) */
	buffer->fill(BufferSize / 2);

	/* point both global hints to this buffer */
	m_leftGlobalHint.ui.store(GlobalHint(buffer, 0), std::memory_order_release);
	m_rightGlobalHint.ui.store(GlobalHint(buffer, 0), std::memory_order_release);

	m_pThreadLogs = (padded<ThreadLog>*)memalign(CACHE_LINE_SIZE, sizeof(padded<ThreadLog>) * threadCount);

	for (int i = 0; i < threadCount; ++i) {
		ThreadLog &log = m_pThreadLogs[i].ui;
		log.m_elimPops = 0;
		log.m_elimPushes = 0;
		log.m_oracleInvokes = 0;
		log.m_oracleLoops = 0;
	}
}

template<typename T, int BufferSize, bool Elimination>
OFDeque<T, BufferSize, Elimination>::~OFDeque() {
	// delete m_pBlockPool;
  
  free(m_pHazTracker);
  free(m_pLeftBufferCache);
  free(m_pRightBufferCache);
  free(m_pLeftElimTable);
  free(m_pRightElimTable);
  free(m_pThreadLogs);
}

template<typename T, int BufferSize, bool Elimination>
void OFDeque<T, BufferSize, Elimination>::left_push(T value, int tid) {
	doPush<OFDequeTypes::SIDE_LEFT>(value, tid);
}

template<typename T, int BufferSize, bool Elimination>
void OFDeque<T, BufferSize, Elimination>::right_push(T value, int tid) {
	doPush<OFDequeTypes::SIDE_RIGHT>(value, tid);
}

template<typename T, int BufferSize, bool Elimination>
T OFDeque<T, BufferSize, Elimination>::left_pop(int tid) {
	return doPop<OFDequeTypes::SIDE_LEFT>(tid);
}

template<typename T, int BufferSize, bool Elimination>
T OFDeque<T, BufferSize, Elimination>::right_pop(int tid) {
	return doPop<OFDequeTypes::SIDE_RIGHT>(tid);
}

template<typename T, int BufferSize, bool Elimination>
template<OFDequeTypes::Side S>
void OFDeque<T, BufferSize, Elimination>::doPush(const T &value, int tid) {
	using namespace OFDequeTypes;
	
	int backoffScanCount = m_scanCountStart;
	if (Elimination) {
		getElimTable<S>()->insertPush(value, tid);
	}

	for (;;) {
		OracleResult oracleResult = oracle<S>(tid);
		
		if (Elimination) {
			if (getElimTable<S>()->removePush(tid)) {
				goto elim_out;
			}
		}

		Buffer *buffer = oracleResult.m_edge.m_pBuffer;
		int nearIndex = oracleResult.m_edge.m_index;
		int farIndex = nearIndex + GetFarDirection<S>();

		Slot nearSlot = buffer->m_pSlots[nearIndex].load(std::memory_order_acquire);
		Slot farSlot = buffer->m_pSlots[farIndex].load(std::memory_order_acquire);

		Type nearType = (Type)nearSlot.m_type;
		Type farType = (Type)farSlot.m_type;

		/* check oracle edge */
		if (nearType == GetFarType<S>() || (nearType == TYPE_SEALED && nearIndex != GetFarValueIndex<S>())) {
			goto backoff;
    }

		if (farIndex != GetFarLinkIndex<S>()) {
			if (farType != GetFarType<S>()) {
				goto backoff;
			}
		}
		if (nearIndex == GetNearLinkIndex<S>()) {
			if (nearType != GetNearType<S>()) {
				goto backoff;
			}
		}

		if (nearIndex != GetFarValueIndex<S>()) {
			/* interior push */
			if (buffer->casSafe(nearIndex, nearSlot)) {
				if (buffer->casValue(farIndex, farSlot, value)) {
					/* update interior hint */
					GetLocalHint<S>(buffer).fetch_add(GetFarDirection<S>(), std::memory_order_acq_rel);
					goto out;
				}
			}
		} else {
			/* either straddling or boundary */
			if (farType == GetFarType<S>()) {
				/* append */
				/* grab new buffer from cache */
				Buffer *newBuffer = getBufferCache<S>()[tid].ui;
				if (newBuffer == NULL) {
					/* setup new buffer if needed */
					newBuffer = m_pBlockPool->alloc(tid);
					newBuffer->m_leftLocalHint.ui.store(GetNearValueIndex<S>(), std::memory_order_relaxed);
					newBuffer->m_rightLocalHint.ui.store(GetNearValueIndex<S>(), std::memory_order_relaxed);

					for (int i = 0; i < BufferSize; ++i) {
						Slot s;
						s.m_type = GetFarType<S>();
						newBuffer->m_pSlots[i].store(s, std::memory_order_relaxed);
					}

					getBufferCache<S>()[tid].ui = newBuffer;
				}

				/* pre-insert link and near value node with @value */
				Slot s1;
				s1.m_pLink = buffer;
				s1.m_type = TYPE_VALUE;

				Slot s2;
				s2.m_value = value;
				s2.m_type = TYPE_VALUE;

				newBuffer->m_pSlots[GetNearLinkIndex<S>()].store(s1, std::memory_order_relaxed);
				newBuffer->m_pSlots[GetNearValueIndex<S>()].store(s2, std::memory_order_relaxed);

				/* try append */
				if (buffer->casSafe(nearIndex, nearSlot)) {
					if (buffer->casLink(farIndex, farSlot, newBuffer)) {
						/* clear buffer cache */
						getBufferCache<S>()[tid].ui = NULL;
						/* update global hint */
						getGlobalHint<S>().compare_exchange_strong(oracleResult.m_hint, GlobalHint(newBuffer, oracleResult.m_hint.m_count + 1), std::memory_order_acq_rel, std::memory_order_acquire);
						goto out;
					}
				}
			} else {
				/* either straddling push or help remove sealed buffer */
				Buffer *neighbor = farSlot.m_pLink;
				Slot reachingSlot = neighbor->m_pSlots[GetNearValueIndex<S>()].load(std::memory_order_acquire);

				/* make sure far neighbor points back to buffer */
				Slot backSlot = neighbor->m_pSlots[GetNearLinkIndex<S>()].load(std::memory_order_acquire);

				if (backSlot.m_pLink != buffer) {
					goto backoff;
        }

				Type reachingType = (Type)reachingSlot.m_type;
				if (reachingType == GetFarType<S>()) {
					/* straddling push */
					if (buffer->casSafe(nearIndex, nearSlot)) {
						if (neighbor->casValue(GetNearValueIndex<S>(), reachingSlot, value)) {
							/* update global hint */
							getGlobalHint<S>().compare_exchange_strong(oracleResult.m_hint, GlobalHint(neighbor, oracleResult.m_hint.m_count + 1), std::memory_order_acq_rel, std::memory_order_acquire);
							goto out;
						}
					}
				} else if (reachingType == Type::TYPE_SEALED) {
					/* remove sealed neighbor */
					if (buffer->casSafe(nearIndex, nearSlot)) {
						if (buffer->casType(farIndex, farSlot, GetFarType<S>())) {
							retire(neighbor, tid);
							goto backoff;
						}
					}
				}
			}
		}
	backoff:
		if (Elimination) {
			getElimTable<S>()->insertPush(value, tid);
			if (getElimTable<S>()->tryEliminatePush(backoffScanCount, value, tid)) {
				goto elim_out;
			}
			backoffScanCount <<= 1;
		}
	}
elim_out:
	m_pThreadLogs[tid].ui.m_elimPushes++;
out:
	m_pHazTracker->clearAll(tid);
}

template<typename T, int BufferSize, bool Elimination>
template<OFDequeTypes::Side S>
T OFDeque<T, BufferSize, Elimination>::doPop(int tid) {
	using namespace OFDequeTypes;

	int backoffScanCount = m_scanCountStart;

	if (Elimination) {
		getElimTable<S>()->insertPop(tid);
	}

	T value;
	for (;;) {
		OracleResult oracleResult = oracle<S>(tid);

		if (Elimination) {
			if (getElimTable<S>()->removePop(value, tid)) {
				goto elim_out;
			}
		}

		Buffer *buffer = oracleResult.m_edge.m_pBuffer;
		int nearIndex = oracleResult.m_edge.m_index;
		int farIndex = nearIndex + GetFarDirection<S>();

		Slot nearSlot = buffer->m_pSlots[nearIndex].load(std::memory_order_acquire);
		Slot farSlot = buffer->m_pSlots[farIndex].load(std::memory_order_acquire);

		Type nearType = (Type)nearSlot.m_type;
		Type farType = (Type)farSlot.m_type;

		/* check oracle edge */
		if (nearType == GetFarType<S>() || (nearType == TYPE_SEALED && nearIndex != GetFarValueIndex<S>())) {
			goto backoff;
    }
  
		if (farIndex != GetFarLinkIndex<S>()) {
			if (farType != GetFarType<S>()) {
				goto backoff;
			}
		}

		if (nearIndex == GetNearLinkIndex<S>()) {
			if (nearType != GetNearType<S>()) {
				goto backoff;
			}
		}

		if (nearIndex != GetFarValueIndex<S>()) {
			/* interior edge */
		
			/* check empty */
			if (nearType == GetNearType<S>() && buffer->m_pSlots[nearIndex].load(std::memory_order_acquire).m_count == nearSlot.m_count) {
				value = m_empty;
				goto out;
			}

			if (buffer->casSafe(farIndex, farSlot)) {
				if (buffer->casType(nearIndex, nearSlot, GetFarType<S>())) {
					/* update local hint */
					GetLocalHint<S>(buffer).fetch_add(-GetFarDirection<S>(), std::memory_order_acq_rel);
					value = nearSlot.m_value;
					goto out;
				}
			}
		} else {
			/* edge is on border */
			/* check if straddling edge */
			if (farType != GetFarType<S>()) {
				Buffer *neighbor = farSlot.m_pLink;
				Slot reachSlot = neighbor->m_pSlots[GetNearValueIndex<S>()].load(std::memory_order_acquire);

				/* check neighbor points back */
				Slot backSlot = neighbor->m_pSlots[GetNearLinkIndex<S>()].load(std::memory_order_acquire);

				if (backSlot.m_pLink != buffer) {
					goto backoff;
        }

				/* try seal (if necessary) */
				if (reachSlot.m_type == GetFarType<S>()) {
					/* check empty */
					if ((nearType == GetNearType<S>() || nearType == Type::TYPE_SEALED) &&
							nearSlot.m_count == buffer->m_pSlots[nearIndex].load(std::memory_order_acquire).m_count) {
						value = m_empty;
						goto out;
					}
			
					if (buffer->casSafe(nearIndex, nearSlot)) {
						if (neighbor->casType(GetNearValueIndex<S>(), reachSlot, Type::TYPE_SEALED)) {
							reachSlot.m_type = Type::TYPE_SEALED;
							reachSlot.m_count++;
						}
						nearSlot.m_count++;
					}
				}

				/* remove far buffer */
				if (reachSlot.m_type == Type::TYPE_SEALED) {
					/* check empty */
					if (nearSlot.m_type == GetNearType<S>() && nearSlot.m_count == buffer->m_pSlots[nearIndex].load(std::memory_order_acquire).m_count) {
						value = m_empty;
						goto out;
					}

					if (buffer->casSafe(nearIndex, nearSlot)) {
						if (buffer->casType(farIndex, farSlot, GetFarType<S>())) {
							/* retire neighbor */
							retire(neighbor, tid);
							farSlot.m_type = GetFarType<S>();
							farSlot.m_count++;
						}
						nearSlot.m_count++;
					}
				}
			}

			/* check for boundary edge and boundary pop */
			if (farSlot.m_type == GetFarType<S>()) {
				/* check empty */
				if (nearSlot.m_type == GetNearType<S>() && nearSlot.m_count == buffer->m_pSlots[nearIndex].load(std::memory_order_acquire).m_count) {
					value = m_empty;
					goto out;
				}

				if (buffer->casSafe(farIndex, farSlot)) {
					if (buffer->casType(nearIndex, nearSlot, GetFarType<S>())) {
						/* update global hint */
						getGlobalHint<S>().compare_exchange_strong(oracleResult.m_hint, GlobalHint(buffer, oracleResult.m_hint.m_count + 1), std::memory_order_acq_rel, std::memory_order_acquire);
						value = nearSlot.m_value;
						goto out;
					}
				}
			}
		}
	backoff:
		if (Elimination) {
			getElimTable<S>()->insertPop(tid);
			if (getElimTable<S>()->tryEliminatePop(backoffScanCount, value, tid)) {
				goto elim_out;
			}
			backoffScanCount <<= 1;
		}
	}
elim_out:
	m_pThreadLogs[tid].ui.m_elimPops++;
out:
	m_pHazTracker->clearAll(tid);
	return value;
}

template<typename T, int BufferSize, bool Elimination>
template<OFDequeTypes::Side S>
typename OFDeque<T, BufferSize, Elimination>::OracleResult OFDeque<T, BufferSize, Elimination>::oracle(int tid) {
	using namespace OFDequeTypes;
	
	OracleResult result;
	for (;;) {
		GlobalHint hint = reserveHint<S>(0, tid);
		if (findEdge<S>(result.m_edge, hint, tid)) {
			result.m_hint = hint;
			break;
		}
	}

	return result;
}

template<typename T, int BufferSize, bool Elimination>
template<OFDequeTypes::Side S>
bool OFDeque<T, BufferSize, Elimination>::findEdge(Edge &outEdge, GlobalHint hint, int tid) {
	using namespace OFDequeTypes;
	
	Buffer *buffer = hint.m_pBuffer;
	int index = GetLocalHint<S>(buffer).load(std::memory_order_acquire);
	
	/*
	* because FAI updates to the local hints could occur in arbitrary orders,
	* we could have a window where the index is 'invalid' (< 0 or >= BufferSize)
	*/
	index = (index < 1) ? 1 : (index >= BufferSize - 1) ? BufferSize - 2 : index;

	int nextHazSlot = 1;
	Buffer *neighbor;
	Slot slot;
	Type type, typeFar;

	for (;;) {
		switch (index) {
		case GetFarLinkIndex<S>() :
			slot = buffer->loadSlot(index);

			if (slot.m_type == GetFarType<S>()) {
				index -= GetFarDirection<S>();
			} else {
				neighbor = slot.m_pLink;
				
				m_pHazTracker->reserve(neighbor, nextHazSlot, tid);
				nextHazSlot = !nextHazSlot;

				/* if hint has changed, return false */
				if (hint.m_count != getGlobalHint<S>().load(std::memory_order_acquire).m_count) {
					return false;
        }

				typeFar = neighbor->loadType(GetNearValueIndex<S>());

				if (typeFar == GetFarType<S>() || typeFar == Type::TYPE_SEALED) {
					outEdge = Edge(buffer, GetFarValueIndex<S>());
					return true;
				}

				buffer = neighbor;
				index = GetLocalHint<S>(neighbor).load(std::memory_order_acquire);
				index = (index < 1) ? 1 : (index >= BufferSize - 1) ? BufferSize - 2 : index;
			}
			break;
		case GetNearLinkIndex<S>() :
			slot = buffer->loadSlot(index);

			if (slot.m_type == GetNearType<S>()) {
				/* found the far edge */
				if (buffer->loadType(index + GetFarDirection<S>()) == GetFarType<S>()) {
					outEdge = Edge(buffer, index);
					return true;
				}

				index += GetFarDirection<S>();
			} else {
				assert(slot.m_type == TYPE_VALUE);

				neighbor = slot.m_pLink;

				m_pHazTracker->reserve(neighbor, nextHazSlot, tid);
				nextHazSlot = !nextHazSlot;

				/* if hint has changed, restart from the top */
				if (hint.m_count != getGlobalHint<S>().load(std::memory_order_acquire).m_count) {
					return false;
        }

				typeFar = neighbor->loadType(GetFarValueIndex<S>());

				if (typeFar != GetFarType<S>()) {
					outEdge = Edge(neighbor, GetFarValueIndex<S>());
					return true;
				}

				buffer = neighbor;
				index = GetLocalHint<S>(neighbor).load(std::memory_order_acquire);
				index = (index < 1) ? 1 : (index >= BufferSize - 1) ? BufferSize - 2 : index;
			}
			break;
		default:
			type = buffer->loadType(index);
			switch (type) {
			case GetFarType<S>():
				index -= GetFarDirection<S>();
				break;
			case GetNearType<S>():
			case Type::TYPE_VALUE:
				/* found the far edge */
				if (buffer->loadType(index + GetFarDirection<S>()) == GetFarType<S>()) {
					outEdge = Edge(buffer, index);
					return true;
				}

				index += GetFarDirection<S>();
				break;
			case Type::TYPE_SEALED:
				/* check if the near or far value node is sealed */
				switch (index) {
				case GetFarValueIndex<S>() :
					neighbor = buffer->loadSlot(GetFarLinkIndex<S>()).m_pLink;
					
					m_pHazTracker->reserve(neighbor, nextHazSlot, tid);
					nextHazSlot = !nextHazSlot;

					/* if hint has changed, return false */
					if (hint.m_count != getGlobalHint<S>().load(std::memory_order_acquire).m_count) {
						return false;
          }

					typeFar = neighbor->loadType(GetNearValueIndex<S>());

					if (typeFar == GetFarType<S>()) {
						outEdge = Edge(buffer, index);
						return true;
					}

					buffer = neighbor;
					index = GetLocalHint<S>(neighbor).load(std::memory_order_acquire);
					index = (index < 1) ? 1 : (index >= BufferSize - 1) ? BufferSize - 2 : index;
					break;
				case GetNearValueIndex<S>() :
					neighbor = buffer->loadSlot(GetNearLinkIndex<S>()).m_pLink;
					
					m_pHazTracker->reserve(neighbor, nextHazSlot, tid);
					nextHazSlot = !nextHazSlot;

					/* if hint has changed, return false */
					if (hint.m_count != getGlobalHint<S>().load(std::memory_order_acquire).m_count) {
						return false;
          }

					typeFar = neighbor->loadType(GetFarValueIndex<S>());

					if (typeFar == GetNearType<S>() || typeFar == Type::TYPE_VALUE) {
						outEdge = Edge(neighbor, GetFarValueIndex<S>());
						return true;
					}

					buffer = neighbor;
					index = GetLocalHint<S>(neighbor).load(std::memory_order_acquire);
					index = (index < 1) ? 1 : (index >= BufferSize - 1) ? BufferSize - 2 : index;
					break;
				default:
					assert(0);
				}
				break;
			default:
				assert(0);
			}
			break;
		}
	}
}

template<typename T, int BufferSize, bool Elimination>
void OFDeque<T, BufferSize, Elimination>::retire(Buffer *buffer, int tid) {
	using namespace OFDequeTypes;

	/* update left hint */
	updateHint<SIDE_LEFT>(tid);

	/* update right hint */
	updateHint<SIDE_RIGHT>(tid);

	/* now we can retire the buffer */
	m_pHazTracker->retire(buffer, tid);
}

template<typename T, int BufferSize, bool Elimination>
template<OFDequeTypes::Side S>
typename OFDeque<T, BufferSize, Elimination>::GlobalHint OFDeque<T, BufferSize, Elimination>::reserveHint(int slot, int tid) {
	for (;;) {
		GlobalHint hint = getGlobalHint<S>().load(std::memory_order_acquire);

		m_pHazTracker->reserve(hint.m_pBuffer, slot, tid);

		if (hint.m_count == getGlobalHint<S>().load(std::memory_order_acquire).m_count) {
			return hint;
		}
	}
}

template<typename T, int BufferSize, bool Elimination>
template<OFDequeTypes::Side S>
void OFDeque<T, BufferSize, Elimination>::updateHint(int tid) {
	uint32_t threshold = getGlobalHint<S>().load(std::memory_order_acquire).m_count;

	for (;;) {
		GlobalHint hint = getGlobalHint<S>().load(std::memory_order_acquire);

		Buffer *buffer = hint.m_pBuffer;

		if (buffer->isSealed() == -1) {
			/* if the buffer is not sealed and the hint has changed, someone else has done our work */
			if (hint.m_count > threshold) {
				break;
      }
		} else {
			/* if findActiveBuffer(...) returns false, the hint has changed */
			if (!findActiveBuffer<S>(&buffer, hint, tid)) {
				continue;
			}

			assert(buffer != NULL);
		}

		if (getGlobalHint<S>().compare_exchange_strong(hint, GlobalHint(buffer, hint.m_count + 1), std::memory_order_acq_rel, std::memory_order_acquire)) {
			break;
		}
	}
}

template<typename T, int BufferSize, bool Elimination>
template<OFDequeTypes::Side S>
bool OFDeque<T, BufferSize, Elimination>::findActiveBuffer(Buffer **outBuffer, GlobalHint hint, int tid) {
	using namespace OFDequeTypes;

	int nextHazSlot = 1;

	Buffer *buffer = hint.m_pBuffer;

	for (;;) {
		int sealedIndex = buffer->isSealed();
	
		Slot slot;
		switch (sealedIndex) {
		case -1:
			*outBuffer = buffer;
			return true;
		case GetFarValueIndex<S>():
			slot = buffer->loadSlot(GetFarLinkIndex<S>());			
			break;
		case GetNearValueIndex<S>():
			slot = buffer->loadSlot(GetNearLinkIndex<S>());
			break;
		default:
			assert(0);
		}

		m_pHazTracker->reserve(slot.m_pLink, nextHazSlot, tid);
		nextHazSlot = !nextHazSlot;		

		/* if hint has changed, return false */
		if (hint.m_count != getGlobalHint<S>().load(std::memory_order_acquire).m_count) {
			return false;
    }

		buffer = slot.m_pLink;
	}
}

template<typename T, int BufferSize, bool Elimination> 
void OFDeque<T, BufferSize, Elimination>::Buffer::fill(int split) {
	assert(split >= 0 && split < BufferSize);

	m_leftLocalHint.ui.store(split, std::memory_order_relaxed);
	m_rightLocalHint.ui.store(split - 1, std::memory_order_relaxed);

	for (int i = 0; i < split; ++i) {
		Slot s;
		s.m_type = OFDequeTypes::TYPE_LEFT;
		m_pSlots[i].store(s, std::memory_order_relaxed);
	}

	for (int i = split; i < BufferSize; ++i) {
		Slot s;
		s.m_type = OFDequeTypes::TYPE_RIGHT;
		m_pSlots[i].store(s, std::memory_order_relaxed);
	}
}

template<typename T, int BufferSize, bool Elimination>
int OFDeque<T, BufferSize, Elimination>::Buffer::isSealed() {
	using namespace OFDequeTypes;

	Slot n0, n1;

	for (;;) {
		n0 = m_pSlots[1].load(std::memory_order_acquire);
		n1 = m_pSlots[BufferSize - 2].load(std::memory_order_acquire);

		if (n0.m_count == m_pSlots[1].load(std::memory_order_acquire).m_count) {
			if (n0.m_type == TYPE_SEALED) {
				return 1;
			} else if (n1.m_type == TYPE_SEALED) {
				return BufferSize - 2;
			} else {
				return -1;
			}
		}
	}
}

template<typename T, int BufferSize, bool Elimination>
template<OFDequeTypes::Side S> 
constexpr int OFDeque<T, BufferSize, Elimination>::GetFarLinkIndex() { 
	return OFDequeUtils<S, T, BufferSize, Elimination>::GetFarLinkIndex(); 
}

template<typename T, int BufferSize, bool Elimination>
template<OFDequeTypes::Side S> 
constexpr int OFDeque<T, BufferSize, Elimination>::GetNearLinkIndex() {
	return OFDequeUtils<S, T, BufferSize, Elimination>::GetNearLinkIndex();
}

template<typename T, int BufferSize, bool Elimination>
template<OFDequeTypes::Side S>
constexpr int OFDeque<T, BufferSize, Elimination>::GetFarValueIndex() {
	return OFDequeUtils<S, T, BufferSize, Elimination>::GetFarValueIndex();
}

template<typename T, int BufferSize, bool Elimination>
template<OFDequeTypes::Side S>
constexpr int OFDeque<T, BufferSize, Elimination>::GetNearValueIndex() {
	return OFDequeUtils<S, T, BufferSize, Elimination>::GetNearValueIndex();
}

template<typename T, int BufferSize, bool Elimination>
template<OFDequeTypes::Side S>
std::atomic<int> &OFDeque<T, BufferSize, Elimination>::GetLocalHint(Buffer *buf) {
	return OFDequeUtils<S, T, BufferSize, Elimination>::GetLocalHint(buf);
}


template<typename T, int BufferSize, bool Elimination>
template<OFDequeTypes::Side S> 
constexpr int OFDeque<T, BufferSize, Elimination>::GetFarDirection() {
	return OFDequeUtils<S, T, BufferSize, Elimination>::GetFarDirection();
}

template<typename T, int BufferSize, bool Elimination>
template<OFDequeTypes::Side S> 
constexpr OFDequeTypes::Type OFDeque<T, BufferSize, Elimination>::GetFarType() {
	return OFDequeUtils<S, T, BufferSize, Elimination>::GetFarType();
}

template<typename T, int BufferSize, bool Elimination>
template<OFDequeTypes::Side S>
constexpr OFDequeTypes::Type OFDeque<T, BufferSize, Elimination>::GetNearType() {
	return OFDequeUtils<S, T, BufferSize, Elimination>::GetNearType();
}

template<typename T, int BufferSize, bool Elimination>
template<OFDequeTypes::Side S> 
std::atomic<typename OFDeque<T, BufferSize, Elimination>::GlobalHint> &OFDeque<T, BufferSize, Elimination>::getGlobalHint() {
	return OFDequeUtils<S, T, BufferSize, Elimination>::GetGlobalHint(this);
}

template<typename T, int BufferSize, bool Elimination>
template<OFDequeTypes::Side S> 
padded<typename OFDeque<T, BufferSize, Elimination>::Buffer*> *OFDeque<T, BufferSize, Elimination>::getBufferCache() {
	return OFDequeUtils<S, T, BufferSize, Elimination>::GetBufferCache(this);
}

template<typename T, int BufferSize, bool Elimination>
template<OFDequeTypes::Side S>
ElimTable<T> *OFDeque<T, BufferSize, Elimination>::getElimTable() {
	return OFDequeUtils<S, T, BufferSize, Elimination>::GetElimTable(this);
}

#endif
