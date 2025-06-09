/**
 * @file stack_impl.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2025-05-09
 *
 * @copyright Copyright (c) 2025
 *
 */
#ifndef STACK_IMPL_H
#define STACK_IMPL_H

#include "record_manager.h"
#include "ts_stack.h"


#include <gflags/gflags.h>

#include "datastructures/ts_timestamp.h"
#include "ts_stack_buffer.h"
#include "datastructures/ts_stack.h"

DEFINE_uint64(delay, 0, "delay in the insert operation");

#define TS_DS TSStack<uint64_t, TSStackBuffer<uint64_t, HardwareTimestamp>, HardwareTimestamp>

TS_DS *ts_;

void* ds_new() {
  ts_ = new TS_DS(g_num_threads + 1, FLAGS_delay);
  return static_cast<void*>(ts_);
}

char* ds_get_stats(void) {
  return ts_->ds_get_stats();
}

template <typename K, typename V>
class node_t {
   public:
    K key;
    V val;
    std::atomic<node_t<K, V> *> next;
};
#define nodeptr node_t<K, V> *

template <typename K, typename V, class RecManager>
class Stack_WRAP {
   private:
   public:
    Stack_WRAP(const int num_threads, const int _min_key, const int _max_key,
               const V _NO_VALUE, unsigned int id) {}

    ~Stack_WRAP() {}

    V peek(const int &tid) { return NULL; }

    bool push(const int &tid, const V &value) { return true; }

    bool pop(const int &tid) { return true; }
};

#endif