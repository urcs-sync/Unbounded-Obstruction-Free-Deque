// Copyright (c) 2012-2013, the Scal Project Authors.  All rights reserved.
// Please see the AUTHORS file for details.  Use of this source code is governed
// by a BSD license that can be found in the LICENSE file.

#ifndef SCAL_DATASTRUCTURES_TS_DEQUE_BUFFER_H_
#define SCAL_DATASTRUCTURES_TS_DEQUE_BUFFER_H_

#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include <assert.h>
#include <atomic>
#include <stdio.h>

#include "util/threadlocals.h"
#include "util/random.h"
#include "util/malloc.h"
#include "util/platform.h"

#include "BlockPool.hpp"
#include "ConcurrentPrimitives.hpp"

template<typename T, typename TimeStamp>
class TSDequeBuffer { 
  private:

    struct Item;

    typedef struct ItemPtr {
      static inline ItemPtr get(Item *item) {
        ItemPtr p; 
        p.item = item;
        return p;
      }
      
      static inline ItemPtr get(Item *item, uint32_t cntr) {
        ItemPtr p; 
        p.item = item; p.cntr = cntr;
        return p;
      }

      inline bool operator!=(const ItemPtr& o) {
        return *((uint64_t*)&item) != *((uint64_t*)&o.item); 
      }

      Item *item;
      uint32_t cntr;
    } ItemPtr;

    typedef struct Item {
      std::atomic<Item*> left;
      std::atomic<Item*> right;
      std::atomic<uint64_t> taken;
      std::atomic<T> data;
      std::atomic<uint64_t> timestamp[2];
      // Insertion index, needed for the termination condition in 
      // get_left_item. Items inserted at the left get negative
      // indices, items inserted at the right get positive indices.
      std::atomic<int64_t> index;
    } Item;

    // The number of threads.
    uint64_t num_threads_;
    
    paddedAtomic<ItemPtr> *left2_;
    //std::atomic<Item*> **left_;
    
    paddedAtomic<ItemPtr> *right2_;
    //std::atomic<Item*> **right_;
    
    padded<int64_t> *next_index2_;
    //int64_t **next_index_;

    // The pointers for the emptiness check.
    padded<ItemPtr*> *emptiness_check_left2_;
    //Item** *emptiness_check_left_;

    padded<ItemPtr*> *emptiness_check_right2_;
    //Item** *emptiness_check_right_;
    
    TimeStamp *timestamping_;
    
    padded<uint64_t> *counter12_;
    //uint64_t* *counter1_;

    padded<uint64_t> *counter22_;
    //uint64_t* *counter2_;
    
    BlockPool<Item> *itemPool_;

    // Helper function to remove the ABA counter from a pointer.
    void *get_aba_free_pointer(void *pointer) {
      uint32_t result = (uint32_t)pointer;
      result &= 0xfffffff8;
      return (void*)result;
    }

    // Helper function which retrieves the ABA counter of a pointer old
    // and sets this ABA counter + increment to the pointer pointer.
    void *add_next_aba(void *pointer, void *old, uint32_t increment) {
      uint32_t aba = (uint32_t)old;
      aba += increment;
      aba &= 0x7;
      uint32_t result = (uint32_t)pointer;
      result = (result & 0xfffffff8) | aba;
      return (void*)((result & 0xfffffff8) | aba);
    }

    // Returns the leftmost not-taken item from the thread-local list 
    // indicated by thread_id.
    Item *get_left_item(int thread_id) {

      // Read the item pointed to by the right pointer. The iteration through
      // the linked list can stop at that item.
      ItemPtr old_right = right2_[thread_id].ui.load();

      // Item* old_right = right_[thread_id]->load();
      Item* right = old_right.item;
      int64_t threshold = right->index.load();

      // Read the leftmost item.
      Item *result = left2_[thread_id].ui.load().item;
      // Item* result = (Item*)get_aba_free_pointer(left_[thread_id]->load());

      // We start at the left pointer and iterate to the right until we
      // find the first item which has not been taken yet.
      while (true) {
        // We reached a node further right than the original right-most 
        // node. We do not have to search any further to the right, we
        // will not take the element anyways.
        if (result->index.load() > threshold) {
          return NULL;
        }
        // We found a good node, return it.
        if (result->taken.load() == 0) {
          return result;
        }
        // We have reached the end of the list and found nothing, so we
        // return NULL.
        if (result->right.load() == result) {
          return NULL;
        }
        result = result->right.load();
      }
    }

    // Returns the rightmost not-taken item from the thread-local list
    // indicated by thread_id.
    Item *get_right_item(int thread_id) {

      // Read the item pointed to by the left pointer. The iteration through
      // the linked list can stop at that item.
      ItemPtr old_left = left2_[thread_id].ui.load();

      //Item* old_left = left_[thread_id]->load();
      Item* left = old_left.item;
      int64_t threshold = left->index.load();

      Item* result = right2_[thread_id].ui.load().item;
      //Item* result = (Item*)get_aba_free_pointer(right_[thread_id]->load());

      // We start at the right pointer and iterate to the left until we
      // find the first item which has not been taken yet.
      while (true) {
        // We reached a node further left than the original left-most 
        // node. We do not have to search any further to the left, we
        // will not take the element anyways.
        if (result->index.load() < threshold) {
          return NULL;
        }
        // We found a good node, return it.
        if (result->taken.load() == 0) {
          return result;
        }
        // We have reached the end of the list and found nothing, so we
        // return NULL.
        if (result->left.load() == result) {
          return NULL;
        }
        result = result->left.load();
      }
    }

  public:

    void initialize(uint64_t num_threads, TimeStamp *timestamping) {

      num_threads_ = num_threads;
      timestamping_ = timestamping;

      itemPool_ = new BlockPool<Item>(num_threads, false);
      itemPool_->preheat(2000);
      
      left2_ = new paddedAtomic<ItemPtr>[num_threads];
      //left_ = static_cast<std::atomic<Item*>**>(
      //    scal::ThreadLocalAllocator::Get().CallocAligned(num_threads_, sizeof(std::atomic<Item*>*), 
      //      scal::kCachePrefetch * 4));

      right2_ = new paddedAtomic<ItemPtr>[num_threads];
      //right_ = static_cast<std::atomic<Item*>**>(
      //    scal::ThreadLocalAllocator::Get().CallocAligned(num_threads_, sizeof(std::atomic<Item*>*), 
      //      scal::kCachePrefetch * 4));

      next_index2_ = new padded<int64_t>[num_threads];
      //next_index_ = static_cast<int64_t**>(
      //    scal::ThreadLocalAllocator::Get().CallocAligned(num_threads_, sizeof(int64_t*), 
      //      scal::kCachePrefetch * 4));

      emptiness_check_left2_ = new padded<ItemPtr*>[num_threads];
      //emptiness_check_left_ = static_cast<Item***>(
      //    scal::ThreadLocalAllocator::Get().CallocAligned(num_threads_, sizeof(Item**), 
      //      scal::kCachePrefetch * 4));

      emptiness_check_right2_ = new padded<ItemPtr*>[num_threads];
      //emptiness_check_right_ = static_cast<Item***>(
      //    scal::ThreadLocalAllocator::Get().CallocAligned(num_threads_, sizeof(Item**), 
      //      scal::kCachePrefetch * 4));

      for (uint64_t i = 0; i < num_threads_; i++) {

        //left_[i] = static_cast<std::atomic<Item*>*>(scal::get<std::atomic<Item*>>(scal::kCachePrefetch * 4));

        //right_[i] = static_cast<std::atomic<Item*>*>(scal::get<std::atomic<Item*>>(scal::kCachePrefetch * 4));

        //next_index_[i] = static_cast<int64_t*>(scal::get<int64_t>(scal::kCachePrefetch * 4));

        // Add a sentinal node.
        Item *new_item = new Item();
        timestamping_->init_sentinel_atomic(new_item->timestamp);
        new_item->data.store(0);
        new_item->taken.store(1);
        new_item->left.store(new_item);
        new_item->right.store(new_item);
        new_item->index.store(0);

        left2_[i].ui.store(ItemPtr::get(new_item));
        right2_[i].ui.store(ItemPtr::get(new_item));
        next_index2_[i].ui = 1;
        //left_[i]->store(new_item);
        //right_[i]->store(new_item);
        //*next_index_[i] = 1;

        emptiness_check_left2_[i].ui = new ItemPtr[num_threads];
        //emptiness_check_left_[i] = static_cast<Item**> (
        //    scal::ThreadLocalAllocator::Get().CallocAligned(num_threads_, sizeof(Item*), 
        //      scal::kCachePrefetch * 4));

        emptiness_check_right2_[i].ui = new ItemPtr[num_threads];
        //emptiness_check_right_[i] = static_cast<Item**> (
        //    scal::ThreadLocalAllocator::Get().CallocAligned(num_threads_, sizeof(Item*), 
        //      scal::kCachePrefetch * 4));
      }

      counter12_ = new padded<uint64_t>[num_threads];
      counter22_ = new padded<uint64_t>[num_threads];
      //counter1_ = static_cast<uint64_t**>(
      //    scal::ThreadLocalAllocator::Get().CallocAligned(num_threads, sizeof(uint64_t*),
      //      scal::kCachePrefetch * 4));
      //counter2_ = static_cast<uint64_t**>(
      //    scal::ThreadLocalAllocator::Get().CallocAligned(num_threads, sizeof(uint64_t*),
      //      scal::kCachePrefetch * 4));

      for (uint64_t i = 0; i < num_threads; i++) {
        counter12_[i].ui = 0;
        counter22_[i].ui = 0;
      //  counter1_[i] = scal::get<uint64_t>(scal::kCachePrefetch * 4);
      //  *(counter1_[i]) = 0;
      //  counter2_[i] = scal::get<uint64_t>(scal::kCachePrefetch * 4);
      //  *(counter2_[i]) = 0;
      }
    }

    inline void inc_counter1(int thread_id, uint64_t value) {
      counter12_[thread_id].ui += value;
      //(*counter1_[thread_id]) += value;
    }
    inline void inc_counter2(int thread_id, uint64_t value) {
      counter22_[thread_id].ui += value;
      //(*counter2_[thread_id]) += value;
    }
    char* ds_get_stats(void) {
      uint64_t sum1 = 0;
      uint64_t sum2 = 1;

      for (uint64_t i = 0; i < num_threads_; i++) {
        sum1 += counter12_[i].ui;
        sum2 += counter22_[i].ui;
        //sum1 += *counter1_[i];
        //sum2 += *counter2_[i];
      }

      char buffer[255] = { 0 };
      uint32_t n = snprintf(buffer,
                            sizeof(buffer),
                            " ,\"c1\": %lu ,\"c2\": %lu",
                            sum1, sum2);
      if (n != strlen(buffer)) {
        fprintf(stderr, "%s: error creating stats string\n", __func__);
        abort();
      }
      char *newbuf = static_cast<char*>(calloc(
          strlen(buffer) + 1, sizeof(*newbuf)));
      return strncpy(newbuf, buffer, strlen(buffer));
    }

    inline std::atomic<uint64_t> *insert_left(int thread_id, T element) {
      //uint64_t thread_id = scal::ThreadContext::get().thread_id();

      // Create a new item.
      Item *new_item = itemPool_->alloc(thread_id);
      timestamping_->init_top_atomic(new_item->timestamp);
      new_item->data.store(element);
      new_item->taken.store(0);
      new_item->left.store(new_item);
      // Items inserted at the left get negative indices. Thereby the
      // order of items in the thread-local lists correspond with the
      // order of indices, and we can use the sign of the index to
      // determine on which side an item has been inserted.
      new_item->index = -(next_index2_[thread_id].ui++);
      //new_item->index = -((*next_index_[thread_id])++);
      
      // Determine leftmost not-taken item in the list. The new item is
      // inserted to the left of that item.
      ItemPtr old_left = left2_[thread_id].ui.load();
      //Item* old_left = left_[thread_id]->load();

      Item* left = old_left.item;
      while (left->right.load() != left 
          && left->taken.load()) {
        left = left->right.load();
      }

      if (left->taken.load() && left->right.load() == left) {
        // The buffer is empty. We have to increase the aba counter of the
        // right pointer too to guarantee that a pending right-pointer
        // update of a remove operation does not make the left and the
        // right pointer point to different lists.

        left = old_left.item;
        left->right.store(left);
        ItemPtr old_right = right2_[thread_id].ui.load();
        right2_[thread_id].ui.store(ItemPtr::get(left, old_right.cntr + 1));
        //Item* old_right = right_[thread_id]->load();
        //right_[thread_id]->store((Item*) add_next_aba(left, old_right, 1));
      }

      // Add the new item to the list.
      new_item->right.store(left);
      left->left.store(new_item);
      left2_[thread_id].ui.store(ItemPtr::get(new_item, old_left.cntr + 1));
      //left_[thread_id]->store((Item*) add_next_aba(new_item, old_left, 1));
 
      // Return a pointer to the timestamp location of the item so that a
      // timestamp can be added.
      return new_item->timestamp;
    }

    /////////////////////////////////////////////////////////////////
    // insert_right
    /////////////////////////////////////////////////////////////////
    inline std::atomic<uint64_t> *insert_right(int thread_id, T element) {
      //uint64_t thread_id = scal::ThreadContext::get().thread_id();

      // Create a new item.
      Item *new_item = itemPool_->alloc(thread_id);
      //Item *new_item = scal::tlget_aligned<Item>(scal::kCachePrefetch);
      timestamping_->init_top_atomic(new_item->timestamp);
      new_item->data.store(element);
      new_item->taken.store(0);
      new_item->right.store(new_item);
      new_item->index = (next_index2_[thread_id].ui++);
      //new_item->index = (*next_index_[thread_id])++;

      // Determine the rightmost not-taken item in the list. The new item is
      // inserted to the right of that item.
      ItemPtr old_right = right2_[thread_id].ui.load();
      //Item* old_right = right_[thread_id]->load();

      Item* right = old_right.item;
      while (right->left.load() != right 
          && right->taken.load()) {
        right = right->left.load();
      }

      if (right->taken.load() && right->left.load() == right) {
        // The buffer is empty. We have to increase the aba counter of the
        // left pointer too to guarantee that a pending left-pointer
        // update of a remove operation does not make the left and the
        // right pointer point to different lists.
        right = old_right.item;
        right->left.store(right);
        ItemPtr old_left = left2_[thread_id].ui.load();
        left2_[thread_id].ui.store(ItemPtr::get(right, old_left.cntr + 1));
        //Item* old_left = left_[thread_id]->load();
        //left_[thread_id]->store( (Item*) add_next_aba(right, old_left, 1)); 
      }

      // Add the new item to the list.
      new_item->left.store(right);
      right->right.store(new_item);

      right2_[thread_id].ui.store(ItemPtr::get(new_item, old_right.cntr + 1));
      //right_[thread_id]->store((Item*) add_next_aba(new_item, old_right, 1));

      // Return a pointer to the timestamp location of the item so that a
      // timestamp can be added.
      return new_item->timestamp;
    }

    // Helper function which returns true if the item was inserted at the left.
    inline bool inserted_left(Item *item) {
      return item->index.load() < 0;
    }

    // Helper function which returns true if the item was inserted at the right.
    inline bool inserted_right(Item *item) {
      return item->index.load() > 0;
    }

    // Helper function which returns true if item1 is more left than item2.
    inline bool is_more_left(Item *item1, uint64_t *timestamp1, Item *item2, uint64_t *timestamp2) {
      if (inserted_left(item2)) {
        if (inserted_left(item1)) {
          return timestamping_->is_later(timestamp1, timestamp2);
        } else {
          return false;
        }
      } else {
        if (inserted_left(item1)) {
          return true;
        } else {
          return timestamping_->is_later(timestamp2, timestamp1);
        }
      }
    }

    // Helper function which returns true if item1 is more right than item2.
    inline bool is_more_right(Item *item1, uint64_t *timestamp1, Item *item2, uint64_t *timestamp2) {
      if (inserted_right(item2)) {
        if (inserted_right(item1)) {
          return timestamping_->is_later(timestamp1, timestamp2);
        } else {
          return false;
        }
      } else {
        if (inserted_right(item1)) {
          return true;
        } else {
          return timestamping_->is_later(timestamp2, timestamp1);
        }
      }
    }

    bool try_remove_left(int thread_id, T *element, uint64_t *invocation_time) {
      inc_counter2(thread_id, 1);
      // Initialize the data needed for the emptiness check.
      //uint64_t thread_id = scal::ThreadContext::get().thread_id();
      ItemPtr *emptiness_check_left = emptiness_check_left2_[thread_id].ui;
      ItemPtr *emptiness_check_right = emptiness_check_right2_[thread_id].ui;
      //Item* *emptiness_check_left = emptiness_check_left_[thread_id];
      //Item* *emptiness_check_right = emptiness_check_right_[thread_id];
      bool empty = true;
      // Initialize the result pointer to NULL, which means that no 
      // element has been removed.
      Item *result = NULL;
      // Indicates the index which contains the youngest item.
      uint64_t buffer_index = -1;
      // Memory on the stack frame where timestamps of items can be stored
      // temporarily.
      uint64_t tmp_timestamp[2][2];
      // Index in the tmp_timestamp array which is not used at the moment.
      uint64_t tmp_index = 1;
      timestamping_->init_sentinel(tmp_timestamp[0]);
      uint64_t *timestamp = tmp_timestamp[0];
      // Stores the value of the remove pointer of a thead-local buffer 
      // before the buffer is actually accessed.
      ItemPtr old_left = ItemPtr::get(NULL);

      // Read the start time of the iteration. Items which were timestamped
      // after the start time and inserted at the right are not removed.
      uint64_t start_time[2];
      timestamping_->read_time(start_time);
      // We start iterating over the thread-local lists at a random index.
      uint64_t start = hwrand();
      // We iterate over all thead-local buffers
      for (uint64_t i = 0; i < num_threads_; i++) {

        uint64_t tmp_buffer_index = (start + i) % num_threads_;
        // We get the remove/insert pointer of the current thread-local buffer.
        ItemPtr tmp_left = left2_[tmp_buffer_index].ui.load();
        //Item* tmp_left = left_[tmp_buffer_index]->load();
        // We get the youngest element from that thread-local buffer.
        Item* item = get_left_item(tmp_buffer_index);
        // If we found an element, we compare it to the youngest element 
        // we have found until now.
        if (item != NULL) {
          empty = false;
          uint64_t *item_timestamp;
          timestamping_->load_timestamp(tmp_timestamp[tmp_index], item->timestamp);
          item_timestamp = tmp_timestamp[tmp_index];

          if (inserted_left(item) && !timestamping_->is_later(invocation_time, item_timestamp)) {
            uint64_t expected = 0;
            if (item->taken.load() == 0 && item->taken.compare_exchange_weak(expected, 1)) {
              // Try to adjust the remove pointer. It does not matter if 
              // this CAS fails.
              left2_[tmp_buffer_index].ui.compare_exchange_weak(tmp_left, ItemPtr::get(item, tmp_left.cntr));
              //left_[tmp_buffer_index]->compare_exchange_weak(tmp_left, (Item*)add_next_aba(item, tmp_left, 0));
              *element = item->data.load();
              return true;
            } else {
              item = get_left_item(tmp_buffer_index);
              if (item != NULL) {
                timestamping_->load_timestamp(tmp_timestamp[tmp_index], item->timestamp);
                item_timestamp = tmp_timestamp[tmp_index];
              }
            }
          }
          
          if (item != NULL && (result == NULL || is_more_left(item, item_timestamp, result, timestamp))) {
            // We found a new leftmost item, so we remember it.
            result = item;
            buffer_index = tmp_buffer_index;
            timestamp = item_timestamp;
            tmp_index ^=1;
            old_left = tmp_left;
           
            // Check if we can remove the element immediately.
            if (inserted_left(result) && !timestamping_->is_later(invocation_time, timestamp)) {
              uint64_t expected = 0;
              if (result->taken.load() == 0) {
                if (result->taken.compare_exchange_weak(
                    expected, 1)) {
                  // Try to adjust the remove pointer. It does not matter if 
                  // this CAS fails.
                  left2_[buffer_index].ui.compare_exchange_weak(old_left, ItemPtr::get(result, old_left.cntr));
                  //left_[buffer_index]->compare_exchange_weak(old_left, (Item*)add_next_aba(result, old_left, 0));

                  *element = result->data.load();
                  return true;
                }
              }
            }
          }
        } else {
          // No element was found, work on the emptiness check.
          if (emptiness_check_left[tmp_buffer_index] 
              != tmp_left) {
            empty = false;
            emptiness_check_left[tmp_buffer_index] = 
              tmp_left;
          }
          ItemPtr tmp_right = right2_[tmp_buffer_index].ui.load();
          //Item* tmp_right = right_[tmp_buffer_index]->load();
          if (emptiness_check_right[tmp_buffer_index] 
              != tmp_right) {
            empty = false;
            emptiness_check_right[tmp_buffer_index] = 
              tmp_right;
          }
        }
      }
      if (result != NULL) {
        if (!timestamping_->is_later(timestamp, start_time)) {
          // The found item was timestamped after the start of the iteration,
          // so it is save to remove it.
          uint64_t expected = 0;
          if (result->taken.load() == 0) {
            if (result->taken.compare_exchange_weak(
                    expected, 1)) {
              // Try to adjust the remove pointer. It does not matter if this 
              // CAS fails.
              left2_[buffer_index].ui.compare_exchange_weak(old_left, ItemPtr::get(result, old_left.cntr));
              //left_[buffer_index]->compare_exchange_weak(old_left, (Item*)add_next_aba(result, old_left, 0));
              *element = result->data.load();
              return true;
            }
          }
        }
      }

      *element = (T)NULL;
      return !empty;
    }

    bool try_remove_right(int thread_id, T *element, uint64_t *invocation_time) {
      inc_counter2(thread_id, 1);
      // Initialize the data needed for the emptiness check.
      //uint64_t thread_id = scal::ThreadContext::get().thread_id();
      ItemPtr *emptiness_check_left = emptiness_check_left2_[thread_id].ui;
      ItemPtr *emptiness_check_right = emptiness_check_right2_[thread_id].ui;
      //Item* *emptiness_check_left = emptiness_check_left_[thread_id];
      //Item* *emptiness_check_right = emptiness_check_right_[thread_id];
      bool empty = true;
      // Initialize the result pointer to NULL, which means that no 
      // element has been removed.
      Item *result = NULL;
      // Indicates the index which contains the youngest item.
      uint64_t buffer_index = -1;
      // Memory on the stack frame where timestamps of items can be stored
      // temporarily.
      uint64_t tmp_timestamp[2][2];
      // Index in the tmp_timestamp array whihc is not used at the moment.
      uint64_t tmp_index = 1;
      timestamping_->init_sentinel(tmp_timestamp[0]);
      uint64_t *timestamp = tmp_timestamp[0];
      // Stores the value of the remove pointer of a thead-local buffer 
      // before the buffer is actually accessed.
      ItemPtr old_right = ItemPtr::get(NULL);

      // Read the start time of the iteration. Items which were timestamped
      // after the start time and inserted at the left are not removed.
      uint64_t start_time[2];
      timestamping_->read_time(start_time);
      // We start iterating over the thread-local lists at a random index.
      uint64_t start = hwrand();
      // We iterate over all thead-local buffers
      for (uint64_t i = 0; i < num_threads_; i++) {

        uint64_t tmp_buffer_index = (start + i) % num_threads_;
        // We get the remove/insert pointer of the current thread-local buffer.
        ItemPtr tmp_right = right2_[tmp_buffer_index].ui.load();
        //Item* tmp_right = right_[tmp_buffer_index]->load();
        // We get the youngest element from that thread-local buffer.
        Item* item = get_right_item(tmp_buffer_index);
        // If we found an element, we compare it to the youngest element 
        // we have found until now.
        if (item != NULL) {
          empty = false;
          uint64_t *item_timestamp;
          timestamping_->load_timestamp(tmp_timestamp[tmp_index], item->timestamp);
          item_timestamp = tmp_timestamp[tmp_index];
          
          if (inserted_right(item) && !timestamping_->is_later(invocation_time, item_timestamp)) {
            uint64_t expected = 0;
            if (item->taken.load() == 0 && item->taken.compare_exchange_weak(expected, 1)) {
              // Try to adjust the remove pointer. It does not matter if 
              // this CAS fails.
              right2_[tmp_buffer_index].ui.compare_exchange_weak(tmp_right, ItemPtr::get(item, tmp_right.cntr));
              //right_[tmp_buffer_index]->compare_exchange_weak(tmp_right, (Item*)add_next_aba(item, tmp_right, 0));
              *element = item->data.load();
              return true;
            } else {
              item = get_right_item(tmp_buffer_index);
              if (item != NULL) {
                timestamping_->load_timestamp(tmp_timestamp[tmp_index], item->timestamp);
                item_timestamp = tmp_timestamp[tmp_index];
              }
            }
          }

          if (item != NULL && (result == NULL || is_more_right(item, item_timestamp, result, timestamp))) {
            // We found a new youngest element, so we remember it.
            result = item;
            buffer_index = tmp_buffer_index;
            timestamp = item_timestamp;
            tmp_index ^=1;
            old_right = tmp_right;
          }
        } else {
          // No element was found, work on the emptiness check.
          if (emptiness_check_right[tmp_buffer_index] 
              != tmp_right) {
            empty = false;
            emptiness_check_right[tmp_buffer_index] = 
              tmp_right;
          }
          ItemPtr tmp_left = left2_[tmp_buffer_index].ui.load();
          //Item* tmp_left = left_[tmp_buffer_index]->load();
          if (emptiness_check_left[tmp_buffer_index] 
              != tmp_left) {
            empty = false;
            emptiness_check_left[tmp_buffer_index] = 
              tmp_left;
          }
        }
      }
      if (result != NULL) {
        if (!timestamping_->is_later(timestamp, start_time)) {
          // The found item was timestamped after the start of the iteration,
          // so it is save to remove it.
          uint64_t expected = 0;
          if (result->taken.load() == 0) {
            if (result->taken.compare_exchange_weak(
                    expected, 1)) {
              // Try to adjust the remove pointer. It does not matter if
              // this CAS fails.
              right2_[buffer_index].ui.compare_exchange_weak(old_right, ItemPtr::get(result, old_right.cntr));
              //right_[buffer_index]->compare_exchange_weak(old_right, (Item*)add_next_aba(result, old_right, 0));
              *element = result->data.load();
              return true;
            }
          }
        }
      }

      *element = (T)NULL;
      return !empty;
    }
};

#endif  // SCAL_DATASTRUCTURES_TS_DEQUE_BUFFER_H_
