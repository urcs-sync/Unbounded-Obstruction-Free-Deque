// Copyright (c) 2012-2013, the Scal Project Authors.  All rights reserved.
// Please see the AUTHORS file for details.  Use of this source code is governed
// by a BSD license that can be found in the LICENSE file.

#ifndef SCAL_DATASTRUCTURES_TS_DEQUE_H_
#define SCAL_DATASTRUCTURES_TS_DEQUE_H_

#include <stdint.h>
#include <atomic>

#include "datastructures/pool.h"
#include "util/malloc.h"
#include "util/platform.h"
#include "util/random.h"

#include "datastructures/ts_timestamp.h"
#include "datastructures/ts_deque_buffer.h"

#include "RDeque.hpp"

template<typename T, typename TSBuffer, typename Timestamp>
class TSDeque : public RDeque {
  private:
 
    TSBuffer *buffer_;
    Timestamp *timestamping_;

  public:
    TSDeque (uint64_t num_threads, uint64_t delay) {

      //scal::ThreadContext::prepare(num_threads);
      //scal::ThreadContext::assign_context();

      timestamping_ = new Timestamp();
      //timestamping_ = static_cast<Timestamp*>(
      //    scal::get<Timestamp>(scal::kCachePrefetch * 4));

      timestamping_->initialize(delay, num_threads);

      buffer_ = new TSBuffer();
      //buffer_ = static_cast<TSBuffer*>(
      //    scal::get<TSBuffer>(scal::kCachePrefetch * 4));
      buffer_->initialize(num_threads, timestamping_);
    }

    char* ds_get_stats(void) {
      return buffer_->ds_get_stats();
    }

    void left_push(T element, int tid) {
      std::atomic<uint64_t> *item = buffer_->insert_left(tid, element);
      // In the set_timestamp operation first a new timestamp is acquired
      // and then assigned to the item. The operation may not be executed
      // atomically.
      timestamping_->set_timestamp(item);
    }

    void right_push(T element, int tid) {
      std::atomic<uint64_t> *item = buffer_->insert_right(tid, element);
      // In the set_timestamp operation first a new timestamp is acquired
      // and then assigned to the item. The operation may not be executed
      // atomically.
      timestamping_->set_timestamp(item);
    }

    T left_pop(int tid) {
      T element = EMPTY;
      // Read the invocation time of this operation, needed for the
      // elimination optimization.
      uint64_t invocation_time[2];
      timestamping_->read_time(invocation_time);
      while (buffer_->try_remove_left(tid, &element, invocation_time)) {
        if (element != (T)EMPTY) {
          return element;
        }
      }
      // The deque was empty, return false.
      return element;
    }

    T right_pop(int tid) {
      T element = EMPTY;
      // Read the invocation time of this operation, needed for the
      // elimination optimization.
      uint64_t invocation_time[2];
      timestamping_->read_time(invocation_time);
      while (buffer_->try_remove_right(tid, &element, invocation_time)) {
        if (element != (T)EMPTY) {
          return element;
        }
      }
      // The deque was empty, return false.
      return element;
    }
};

class TSDequeFactory : public RContainerFactory {

	int ts;
public:
	enum {HardwareIntervalTS, AtomicCounterTS};
	TSDequeFactory(){ts = HardwareIntervalTS;}
	TSDequeFactory(int timestamp){ts = timestamp;}

  RContainer *build(GlobalTestConfig *gtc) {
	if(ts==HardwareIntervalTS){
    	return new TSDeque<int32_t, TSDequeBuffer<int32_t, HardwareIntervalTimestamp>, HardwareIntervalTimestamp>(gtc->task_num, 10);
	}
	else{
    	return new TSDeque<int32_t, TSDequeBuffer<int32_t, AtomicCounterTimestamp>, AtomicCounterTimestamp>(gtc->task_num, 10);
	}
  }
};

#endif  // SCAL_DATASTRUCTURES_TS_DEQUE_H_

