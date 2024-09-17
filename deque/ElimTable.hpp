#ifndef ELIMTABLE_HPP
#define ELIMTABLE_HPP

#include <atomic>
#include <malloc.h>
#include <cstdlib>
#include <cinttypes>
#include "ConcurrentPrimitives.hpp"

template <typename T>
class ElimTable
{
public:
  ElimTable(int threadCount);

  ~ElimTable();

  void insertPush(const T &value, int tid);
  bool removePush(int tid);
  bool tryEliminatePush(int scanCount, const T &value, int tid);

  void insertPop(int tid);
  bool removePop(T &out, int tid);
  bool tryEliminatePop(int scanCount, T &out, int tid);

private:
  enum Flag
  {
    FLAG_INACTIVE = 0,
    FLAG_ELIMINATED,
    FLAG_PUSH,
    FLAG_POP
  };

  struct Slot
  {
  public:
    Slot() noexcept {}
    Slot(Flag flag) : m_flag(flag) {}
    Slot(const T &value, Flag flag) : m_value(value), m_flag(flag) {}
    T m_value;
    Flag m_flag;
  };

  padded<std::atomic<Slot>> *m_pTable;

  padded<int> *m_pRandNumbers;

  const int m_threadCount;
};

template <typename T>
ElimTable<T>::ElimTable(int threadCount) : m_threadCount(threadCount)
{
  m_pTable = (padded<std::atomic<Slot>> *)memalign(CACHE_LINE_SIZE, sizeof(padded<std::atomic<Slot>>) * threadCount);
  assert(m_pTable);

  for (int i = 0; i < threadCount; ++i)
  {
    m_pTable[i].ui.store(Slot(FLAG_INACTIVE));
  }

  m_pRandNumbers = (padded<int> *)memalign(CACHE_LINE_SIZE, sizeof(padded<int>) * threadCount);
  assert(m_pRandNumbers);

  for (int i = 0; i < threadCount; ++i)
  {
    m_pRandNumbers[i].ui = rand();
  }
}

template <typename T>
ElimTable<T>::~ElimTable()
{
  free(m_pTable);
  free(m_pRandNumbers);
}

template <typename T>
void ElimTable<T>::insertPush(const T &value, int tid)
{
  m_pTable[tid].ui.store(Slot(value, FLAG_PUSH), std::memory_order_release);
}

template <typename T>
void ElimTable<T>::insertPop(int tid)
{
  m_pTable[tid].ui.store(Slot(FLAG_POP), std::memory_order_release);
}

template <typename T>
bool ElimTable<T>::removePush(int tid)
{
  Slot slot = m_pTable[tid].ui.load(std::memory_order_acquire);

  if (slot.m_flag == FLAG_ELIMINATED)
  {
    return true;
  }

  slot = m_pTable[tid].ui.exchange(Slot(FLAG_INACTIVE), std::memory_order_acq_rel);

  return (slot.m_flag == FLAG_INACTIVE);
}

template <typename T>
bool ElimTable<T>::removePop(T &out, int tid)
{
  Slot slot = m_pTable[tid].ui.load(std::memory_order_acquire);

  if (slot.m_flag == FLAG_ELIMINATED)
  {
    out = slot.m_value;
    return true;
  }

  slot = m_pTable[tid].ui.exchange(Slot(FLAG_INACTIVE), std::memory_order_acq_rel);

  if (slot.m_flag == FLAG_INACTIVE)
  {
    out = slot.m_value;
    return true;
  }

  return false;
}

template <typename T>
bool ElimTable<T>::tryEliminatePop(int scanCount, T &out, int tid)
{
  m_pRandNumbers[tid].ui = nextRand(m_pRandNumbers[tid].ui);
  int s = m_pRandNumbers[tid].ui;

  scanCount = (m_threadCount < scanCount) ? m_threadCount : scanCount;

  for (int n = 0; n < scanCount; ++n)
  {
    int i = (s + n) % m_threadCount;

    if (i == tid)
    {
      continue;
    }

    for (;;)
    {
      Slot slot = m_pTable[i].ui.load(std::memory_order_acquire);
      if (slot.m_flag == FLAG_PUSH)
      {
        if (removePop(out, tid))
        {
          return true;
        }

        T value = slot.m_value;

        if (m_pTable[i].ui.compare_exchange_strong(slot, Slot(FLAG_ELIMINATED), std::memory_order_acq_rel, std::memory_order_acquire))
        {
          out = value;
          return true;
        }

        insertPop(tid);
      }
      else
      {
        break;
      }
    }
  }

  return false;
}

template <typename T>
bool ElimTable<T>::tryEliminatePush(int scanCount, const T &value, int tid)
{
  m_pRandNumbers[tid].ui = nextRand(m_pRandNumbers[tid].ui);
  int s = m_pRandNumbers[tid].ui;

  scanCount = (m_threadCount < scanCount) ? m_threadCount : scanCount;

  for (int n = 0; n < scanCount; ++n)
  {
    int i = (s + n) % m_threadCount;

    if (i == tid)
    {
      continue;
    }

    for (;;)
    {
      assert((unsigned)&m_pTable[i] % CACHE_LINE_SIZE == 0);

      Slot slot = m_pTable[i].ui.load(std::memory_order_acquire);

      if (slot.m_flag == FLAG_POP)
      {
        if (removePush(tid))
        {
          return true;
        }

        if (m_pTable[i].ui.compare_exchange_strong(slot, Slot(value, FLAG_ELIMINATED), std::memory_order_acq_rel, std::memory_order_acquire))
        {
          return true;
        }

        insertPush(value, tid);
      }
      else
      {
        break;
      }
    }
  }

  return false;
}

#endif