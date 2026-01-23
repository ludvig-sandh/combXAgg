/**
 * @file stack_impl.h
 * @author Nikosmet Ajay
 * @brief
 * @version 0.1
 * @date 2025-05-09
 *
 * @copyright Copyright (c) 2025
 *
 */
#ifndef STACK_IMPL_H
#define STACK_IMPL_H

#include <ccstack.h>
#include <primitives.h>

#include "record_manager.h"

template <typename K, typename V>
class node_t {
   public:
    K key;
    V val;
    std::atomic<node_t<K, V>*> next;
};
#define nodeptr node_t<K, V>*

class ThreadData {
   private:
    PAD;

   public:
    CCStackThreadState* th_state;

    ThreadData() {}

   private:
    PAD;
};
ThreadData threadData[MAX_THREADS_POW2];
PAD;

template <typename K, typename V, class RecManager>
class Stack {
   private:
    PAD;
    std::atomic<nodeptr> _top;
    PAD;
    CCStackStruct* object_struct CACHE_ALIGN;
    int64_t d1 CACHE_ALIGN, d2;

   public:
    Stack(const int num_threads, const int _min_key, const int _max_key,
          const V _NO_VALUE, unsigned int id)
        : _top(NULL) {
        object_struct = reinterpret_cast<CCStackStruct*>(
            synchGetAlignedMemory(S_CACHE_LINE_SIZE, sizeof(CCStackStruct)));
        CCStackInit(object_struct, num_threads);
    }
    ~Stack() {}

    V peek(const int& tid) { return NULL; }

    bool push(const int& tid, const V& value) {
        CCStackPush(object_struct, threadData[tid].th_state, tid, tid);
        bool success = true;
        return success;
    }

    bool pop(const int& tid) {
        CCStackPop(object_struct, threadData[tid].th_state, tid);
        bool success = true;
        return success;
    }

    void initThread(const int tid) {
        threadData[tid].th_state = reinterpret_cast<CCStackThreadState*>(
            synchGetAlignedMemory(CACHE_LINE_SIZE, sizeof(CCStackThreadState)));
        CCStackThreadStateInit(object_struct, threadData[tid].th_state,
                               (int)tid);
    }

    void deinitThread(const int tid) {}
};

#endif