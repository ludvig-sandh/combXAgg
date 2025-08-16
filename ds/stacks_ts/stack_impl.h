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
#include "ts_stack_buffer.h"
#include "ts_timestamp.h"

#define TS_DS                                                     \
    TSStack<uint64_t, TSStackBuffer<uint64_t, HardwareTimestamp>, \
            HardwareTimestamp>

TS_DS *ts_;

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
    const int _num_threads;
    pthread_barrier_t barrier;

   public:
    Stack_WRAP(const int num_threads, const int _min_key, const int _max_key,
               const V _NO_VALUE, unsigned int id)
        : _num_threads(num_threads) {
        scal::ThreadLocalAllocator::Get().Init(1 * 1024 /* 10mb */, true);
        pthread_key_create(&scal::ThreadContext::threadcontext_key, NULL);
        ts_ = new TS_DS(num_threads, 0);
        pthread_barrier_init(&barrier, NULL, num_threads);
    }

    ~Stack_WRAP() {}

    void initThread(const int tid) {
        VERBOSE COUTATOMIC("begin initThread" << std::endl);
        // const size_t tlsize = scal::HumanSizeToPages("m\n", 10);
        scal::ThreadLocalAllocator::Get().Init(1 * 1024 /* 10mb */, true);
        // COUTATOMIC(" NUM THREADS " << _num_threads << "\n");
        scal::ThreadContext::prepare(_num_threads, tid);
        scal::ThreadContext::assign_context(tid);
        int value = 0;
        ts_->push(value, tid);
        pthread_barrier_wait(&barrier);
        VERBOSE COUTATOMIC("end initThread" << std::endl);
    }

    V peek(const int &tid) {
        ts_->top(tid);
        return NULL;
    }

    bool push(const int &tid, const V &value) {
        ts_->push(value, tid);
        return true;
    }

    bool pop(const int &tid) {
        bool success = false;
        long unsigned int value;
        success = ts_->pop(&value, tid);
        return success;
    }
};

#endif