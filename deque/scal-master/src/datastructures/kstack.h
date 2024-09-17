// Copyright (c) 2012-2013, the Scal Project Authors.  All rights reserved.
// Please see the AUTHORS file for details.  Use of this source code is governed
// by a BSD license that can be found in the LICENSE file.

// Implementing the k-stack from:
//
// T. A. Henzinger, C. M. Kirsch, H. Payer, and A. Sokolova. Quantitative
// relaxation of concurrent data structures. In Proceedings of the 40th annual
// ACM SIGPLAN-SIGACT symposium on Principles of programming languages, POPL
// ’13, New York, NY, USA, 2013. ACM.

#ifndef SCAL_DATASTRUCTURES_KSTACK_H_
#define SCAL_DATASTRUCTURES_KSTACK_H_

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#ifdef LOCALLY_LINEARIZABLE
#include <string.h>
#endif  // LOCALLY_LINEARIZABLE

#include "datastructures/stack.h"
#include "util/allocation.h"
#include "util/atomic_value_new.h"
#include "util/platform.h"
#include "util/random.h"
#include "util/threadlocals.h"

namespace scal {

namespace detail {

template<typename T>
class KSegment : public ThreadLocalMemory<64> {
//class KSegment : public ThreadLocalMemory<128> {
 public:
  typedef TaggedValue<T> Item;
  typedef AtomicTaggedValue<T, 0, 128> AtomicItem;
  typedef TaggedValue<KSegment*> SegmentPtr;
  typedef AtomicTaggedValue<KSegment*, 0, 128> AtomicSegmentPtr;

#ifdef LOCALLY_LINEARIZABLE
  inline void mark() {
    markers[scal::ThreadContext::get().thread_id()].value = 1;
  }

  inline bool is_marked() {
    return markers[scal::ThreadContext::get().thread_id()].value != 0;
  }
#endif  // LOCALLY_LINEARIZABLE

  explicit KSegment(uint64_t k)
      : remove(0),
        next(SegmentPtr(NULL, 0)),
        items(static_cast<AtomicItem*>(
            ThreadLocalAllocator::Get().CallocAligned(
                k, sizeof(*items), 64))) {
#ifdef LOCALLY_LINEARIZABLE
    memset(markers, 0, sizeof(markers));
    const bool checkMarkers = false;
    if (checkMarkers) {
      uint64_t cnt = 0;
      for (uint64_t i = 0; i < kMaxThreads;i++) {
        cnt += markers[i].value;
      }
      assert(cnt == 0);
    }
#endif  // LOCALLY_LINEARIZABLE
  }

  uint8_t remove;
  uint8_t _pad1[63];
  AtomicSegmentPtr  next;
  AtomicItem* items;

#ifdef LOCALLY_LINEARIZABLE
  typedef union {
    uint8_t pad_[16];
    intptr_t value;
  } Marker;

  Marker markers[kMaxThreads];
#endif  // LOCALLY_LINEARIZABLE
};

}  // namespace detail


template<typename T>
class KStack : public Stack<T> {
 public:
  KStack(uint64_t k, uint64_t num_threads);
  bool push(T item);
  bool pop(T *item);

 private:
  typedef detail::KSegment<T> KSegment;
  typedef typename detail::KSegment<T>::Item Item;
  typedef typename detail::KSegment<T>::SegmentPtr SegmentPtr;
  typedef AtomicTaggedValue<KSegment*, 4096, 4096> AtomicTopPtr;

  inline bool is_empty(KSegment* segment);
  inline bool find_index(
      KSegment *segment, bool empty, uint64_t *item_index, TaggedValue<T>* old);
  bool try_add_new_ksegment(const TaggedValue<KSegment*>& top_old, const T& item);
  void try_remove_ksegment(const TaggedValue<KSegment*>& top_old);
  bool committed(
      TaggedValue<KSegment*> top_old, const TaggedValue<T>& item_new, uint64_t index);

  AtomicTopPtr* top_;
  uint64_t k_;
};


template<typename T>
KStack<T>::KStack(uint64_t k, uint64_t num_threads)
    : top_(new AtomicTopPtr(SegmentPtr(new KSegment(k), 0))),
      k_(k) {
}


template<typename T>
bool KStack<T>::is_empty(KSegment* segment) {
  // Distributed Queue style empty check.
  const uint64_t random_index = pseudorand() % k_;
  uint64_t index;
  Item item_old;
  Item old_records[k_];  // NOLINT
  for (uint64_t i = 0; i < k_; i++) {
    index = (random_index + i) % k_;
    item_old = segment->items[index].load();
    if (item_old.value() != (T)NULL) {
      return false;
    } else {
     old_records[index] = item_old;
    }
  }
  for (uint64_t i = 0; i < k_; i++) {
    index = (random_index + i) % k_;
    item_old = segment->items[index].load();
    if (item_old != old_records[index]) {
      return false;
    }
  }
  return true;
}


template<typename T>
bool KStack<T>::try_add_new_ksegment(
    const TaggedValue<KSegment*>& top_old, const T& item) {
  if (top_->load() == top_old) {
    KSegment* segment_new = new KSegment(k_);
    segment_new->items[0].store(Item(item, 0));
    segment_new->next.store(SegmentPtr(top_old.value(), 0));
#ifdef LOCALLY_LINEARIZABLE
    segment_new->mark();
#endif  // LOCALLY_LINEARIZABLE
    if (top_->swap(top_old, SegmentPtr(segment_new, top_old.tag()+ 1))) {
      return true;
    } else {
      delete segment_new;
    }
  }
  return false;
}


template<typename T>
void KStack<T>::try_remove_ksegment(
    const TaggedValue<KSegment*>& top_old) {
  SegmentPtr next = top_->load().value()->next.load();
  if (top_->load() == top_old) {
    if (next.value() != NULL) {
      __sync_fetch_and_add(&top_old.value()->remove, 1);
      if (is_empty(top_old.value())) {
        if (top_->swap(top_old, SegmentPtr(next.value(), top_old.tag() + 1))) {
          return;
        }
      }
      __sync_fetch_and_sub(&top_old.value()->remove, 1);
    }
  }
}


template<typename T>
bool KStack<T>::committed(
    TaggedValue<KSegment*> top_old, const TaggedValue<T>& item_new, uint64_t index) {
  if (top_old.value()->items[index].load() != item_new) {
    return true;
  } else if (top_old.value()->remove == 0) {
    return true;
  } else if (top_old.value()->remove >= 1) {
    if (top_->load() != top_old) {
      if (!top_old.value()->items[index].swap(
            item_new, Item((T)NULL, item_new.tag() + 1))) {
        return true;
      }
    } else {
      if (top_->swap(top_old, SegmentPtr(top_old.value(), top_old.tag() +1))) {
        return true;
      }
      if (!top_old.value()->items[index].swap(
            item_new, Item((T)NULL, item_new.tag() + 1))) {
        return true;
      }
    }
  }
  return false;
}


template<typename T>
bool KStack<T>::find_index(
    KSegment *segment, bool empty, uint64_t *item_index, TaggedValue<T>* old) {
  const uint64_t random_index = hwrand() % k_;
  uint64_t i;
  for (uint64_t _cnt = 0; _cnt < k_; _cnt++) {
    i = (random_index + _cnt) % k_;
    *old = segment->items[i].load();
    if ((empty && old->value() == (T)NULL) ||
        (!empty && old->value() != (T)NULL)) {
      *item_index = i;
      return true;
    }
  }
  return false;
}


template<typename T>
bool KStack<T>::push(T item) {
  TaggedValue<T>::CheckCompatibility(item);
  SegmentPtr top_old;
  Item item_old;
  uint64_t item_index;
  bool found_idx;
  while (true) {
    top_old = top_->load();

#ifdef LOCALLY_LINEARIZABLE
    if (top_old.value()->is_marked()) {
      if (try_add_new_ksegment(top_old, item)) {
        return true;
      }
      continue;
    }
#endif  // LOCALLY_LINEARIZABLE

    found_idx = find_index(top_old.value(), true, &item_index, &item_old);
    if (top_->load() == top_old) {
      if (found_idx) {
        Item item_new(item, item_old.tag() + 1);
        if (top_old.value()->items[item_index].swap(item_old, item_new)) {
          if (committed(top_old, item_new, item_index)) {
#ifdef LOCALLY_LINEARIZABLE
            top_old.value()->mark();
#endif  // LOCALLY_LINEARIZABLE
            return true;
          }
        }
      } else {
        if (try_add_new_ksegment(top_old, item)) {
          return true;
        }
      }
    }
  }
}


template<typename T>
bool KStack<T>::pop(T *item) {
  SegmentPtr top_old;
  Item item_old;
  uint64_t item_index;
  bool found_idx;
  while (true) {
    top_old = top_->load();
    found_idx = find_index(top_old.value(), false, &item_index, &item_old);
    if (top_->load() == top_old) {
      if (found_idx) {
        if (top_old.value()->items[item_index].swap(
              item_old, Item((T)NULL, item_old.tag() + 1))) {
          *item = item_old.value();
          return true;
        }
      } else {
        if (top_old.value()->next.load().value() == NULL) {  // is last segment
          if (is_empty(top_old.value())) {
            if (top_->load() == top_old) {
              return false;
            }
          }
        } else {
          try_remove_ksegment(top_old);
        }
      }
    }
  }
}

}  // namespace scal

#endif  // SCAL_DATASTRUCTURES_KSTACK_H_
