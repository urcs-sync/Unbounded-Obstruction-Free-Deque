
#ifndef FCDEQUE_HPP
#define FCDEQUE_HPP

#include <cassert>
#include <cinttypes>
#include <deque>

#include "RDeque.hpp"
#include "Rideable.hpp"
#include "BlockPool.hpp"
#include "ConcurrentPrimitives.hpp"

template <typename T>
class FCDeque : public RDeque
{
public:
  FCDeque(int threadCount, T empty);

  ~FCDeque();

  void right_push(T value, int tid);
  void left_push(T value, int tid);

  T right_pop(int tid);
  T left_pop(int tid);

private:
  enum REQUEST_STATUS
  {
    RIGHT_PUSH = 0,
    LEFT_PUSH,
    RIGHT_POP,
    LEFT_POP,
    COMPLETE
  };

  struct request_t
  {
    void set(T value, REQUEST_STATUS status)
    {
      this->value = value;
      this->status.store(status, std::memory_order_release);
    }

    void setStatus(REQUEST_STATUS status)
    {
      this->status.store(status, std::memory_order_release);
    }

    REQUEST_STATUS getStatus(std::memory_order o = std::memory_order_acquire)
    {
      return status.load(o);
    }

    volatile T value;
    std::atomic<REQUEST_STATUS> status;
    char pad0[LEVEL1_DCACHE_LINESIZE - sizeof(std::atomic<REQUEST_STATUS>) - sizeof(T)];
  };

  /* --- Instance Methods (Auxiliary) --- */

  bool isLocked();
  bool tryLock(int tid);
  void unlock(int tid);
  void doCombining(int tid);

  /* --- Instance Fields -- */

  std::deque<T> m_deque;
  char pad0[LEVEL1_DCACHE_LINESIZE - sizeof(std::deque<T>)];

  volatile int m_nLock;
  char pad1[LEVEL1_DCACHE_LINESIZE - sizeof(int)];

  request_t *m_pThreadRequests;
  char pad2[LEVEL1_DCACHE_LINESIZE - sizeof(request_t *)];

  request_t ***m_pLeftPop, ***m_pRightPop;

  request_t *m_pThreadLeftPop, *m_pThreadRightPop;

  T m_empty;
  const int m_nThreadCount;

  /* --- Static Fields --- */

  const int MAX_COMBINING_ROUNDS = 10;
  const int COMBINING_LIST_CHECK_FREQ = 10;
};

class FCDequeFactory : public RContainerFactory
{
public:
  RContainer *build(GlobalTestConfig *gtc)
  {
    return new FCDeque<int32_t>(gtc->task_num, EMPTY);
  }
};

/* ---------------------- */
/* --- Implementation --- */
/* ---------------------- */

template <typename T>
FCDeque<T>::FCDeque(int threadCount, T empty) : m_empty(empty),
                                                m_nLock(0),
                                                m_nThreadCount(threadCount)
{
  m_pThreadRequests = new request_t[threadCount];
  assert(m_pThreadRequests);
  for (int i = 0; i < threadCount; ++i)
  {
    m_pThreadRequests[i].set(m_empty, COMPLETE);
  }

  m_pLeftPop = new request_t **[threadCount];
  m_pRightPop = new request_t **[threadCount];
  for (int i = 0; i < threadCount; ++i)
  {
    m_pLeftPop[i] = new request_t *[threadCount];
    m_pRightPop[i] = new request_t *[threadCount];
  }
}

template <typename T>
FCDeque<T>::~FCDeque()
{
  delete[] m_pThreadRequests;
  delete[] m_pLeftPop;
  delete[] m_pRightPop;
}

template <typename T>
void FCDeque<T>::right_push(T value, int tid)
{
  // init request
  request_t *myRequest = m_pThreadRequests + tid;
  myRequest->set(value, RIGHT_PUSH);

  // wait for combining
  int rounds = -1;
  while (myRequest->getStatus() != COMPLETE)
  {
    if (isLocked() == false)
    {
      if (tryLock(tid))
      {
        doCombining(tid);
        unlock(tid);
      }
    }
  }
}

template <typename T>
void FCDeque<T>::left_push(T value, int tid)
{
  // init request
  request_t *myRequest = m_pThreadRequests + tid;
  myRequest->set(value, LEFT_PUSH);

  // wait for combining
  int rounds = -1;
  while (myRequest->getStatus() != COMPLETE)
  {
    if (isLocked() == false)
    {
      if (tryLock(tid))
      {
        doCombining(tid);
        unlock(tid);
      }
    }
  }
}

template <typename T>
T FCDeque<T>::right_pop(int tid)
{
  // init request
  request_t *myRequest = m_pThreadRequests + tid;
  myRequest->setStatus(RIGHT_POP);

  // wait for combining
  int rounds = -1;
  while (myRequest->getStatus() != COMPLETE)
  {
    if (isLocked() == false)
    {
      if (tryLock(tid))
      {
        doCombining(tid);
        unlock(tid);
      }
    }
  }

  return myRequest->value;
}

template <typename T>
T FCDeque<T>::left_pop(int tid)
{
  // init request
  request_t *myRequest = m_pThreadRequests + tid;
  myRequest->setStatus(LEFT_POP);

  // wait for combining
  int rounds = -1;
  while (myRequest->getStatus() != COMPLETE)
  {
    if (isLocked() == false)
    {
      if (tryLock(tid))
      {
        doCombining(tid);
        unlock(tid);
      }
    }
  }

  return myRequest->value;
}

template <typename T>
void FCDeque<T>::doCombining(int tid)
{
  bool finished = false;
  request_t *myRequest = m_pThreadRequests + tid;

  int numLeftPops = 0, numRightPops = 0;
  request_t **leftPoppers = m_pLeftPop[tid];
  request_t **rightPoppers = m_pRightPop[tid];

  for (int j = 0; j < MAX_COMBINING_ROUNDS && !finished; ++j)
  {
    // stage one
    for (int i = 0; i < m_nThreadCount; ++i)
    {
      request_t *req = m_pThreadRequests + i;
      if (req == myRequest)
      {
        finished = true;
      }

      switch (req->getStatus())
      {
      case LEFT_PUSH:
        m_deque.push_front((T)req->value);
        req->setStatus(COMPLETE);
        break;
      case RIGHT_PUSH:
        m_deque.push_back((T)req->value);
        req->setStatus(COMPLETE);
        break;
      case LEFT_POP:
        leftPoppers[numLeftPops++] = req;
        break;
      case RIGHT_POP:
        rightPoppers[numRightPops++] = req;
        break;
      }
    }

    // stage two (left)
    for (int i = 0; i < numLeftPops; ++i)
    {
      T value = m_empty;
      if (!m_deque.empty())
      {
        value = m_deque.front();
        m_deque.pop_front();
      }
      leftPoppers[i]->set(value, COMPLETE);
    }

    // stage two (right)
    for (int i = 0; i < numRightPops; ++i)
    {
      T value = m_empty;
      if (!m_deque.empty())
      {
        value = m_deque.back();
        m_deque.pop_back();
      }
      rightPoppers[i]->set(value, COMPLETE);
    }
  }
}

template <typename T>
bool FCDeque<T>::isLocked()
{
  return m_nLock != 0;
}

template <typename T>
bool FCDeque<T>::tryLock(int tid)
{
  return __sync_lock_test_and_set(&m_nLock, 1) == 0;
}

template <typename T>
void FCDeque<T>::unlock(int tid)
{
  __sync_lock_release(&m_nLock);
}

#endif