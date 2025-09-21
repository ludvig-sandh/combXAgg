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

#include <mutex>

#include "elimination_backoff_stack.h"
#include "record_manager.h"
template <typename K, typename V>
struct node_t {
    K key;
    V val;
};

template <typename K, typename V, class RecManager>
class Stack {
   private:
    PAD;
    scal::EliminationBackoffStack<uint64_t> *ebs;
    PAD;
   uint64_t size_collision=0;
   int nthreads = 0;
   
   public
       : Stack(const int num_threads, const int _min_key, const int _max_key,
               const V _NO_VALUE, unsigned int id) {
        scal::ThreadLocalAllocator::Get().Init(1024, true);
        uint64_t FLAGS_delay = 15000;  //"time waiting in the collision array");
        nthreads = num_threads;
        size_collision = (num_threads + 1) / 10;
        if (size_collision == 0) {
            size_collision = 1;
        }

        COUTATOMIC("STACK CONSTRUCT" << std::endl);
        ebs = new scal::EliminationBackoffStack<uint64_t>(
            num_threads + 1, size_collision, FLAGS_delay);
    }
    ~Stack() {
        COUTATOMIC("DESTROY STACK");
        delete ebs;
        scal::ThreadLocalAllocator::GlobalDestroyAll();
    }

    void initThread(const int tid) {
        scal::ThreadLocalAllocator::Get().Init(1024, true);
    }

    void deinitThread(const int tid) {
    }

    V peek(const int &tid) {
        V retVal;
        return retVal;
    }

    bool push(const int &tid, const K value) {
        bool success = false;
        success = ebs->push(value, tid);
        return success;
    }

    bool pop(const int &tid) {
        bool success = false;
        long unsigned int value;
        success = ebs->pop(&value, tid);
        return success;
    }
};

#endif