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

#include <fcstack.h>
#include <primitives_FC.h>

#include "record_manager.h"

template <typename K, typename V>
class node_t {
   public:
    K key;
    V val;
    std::atomic<node_t<K, V> *> next;
};
#define nodeptr node_t<K, V> *

class ThreadData {
   private:
    PAD;

   public:
    FCStackThreadState *th_state;

    ThreadData() {}

   private:
    PAD;
};

// PAD;
ThreadData threadData[MAX_THREADS_POW2];
PAD;

template <typename K, typename V, class RecManager>
class Stack {
   private:
    PAD;
    std::atomic<nodeptr> _top;
    PAD;
    FCStackStruct *object_struct CACHE_ALIGN;
    int64_t d1 CACHE_ALIGN, d2;
    pthread_barrier_t bar;

   public:
    Stack(const int num_threads, const int _min_key, const int _max_key,
          const V _NO_VALUE, unsigned int id)
        : _top(NULL) {
            pthread_barrier_init(&bar,NULL,num_threads);
        object_struct =
            synchGetAlignedMemory(S_CACHE_LINE_SIZE, sizeof(FCStackStruct));
        FCStackInit(object_struct, num_threads);
    }
    ~Stack() {
        int *int_ptr = synchGetAlignedMemory(S_CACHE_LINE_SIZE, sizeof(int));
    }

    V peek(const int &tid) { return NULL; }

    bool push(const int &tid, const V &value) {
        FCStackPush(object_struct, threadData[tid].th_state, tid, tid);
        bool success = true;
        return success;
    }

    bool pop(const int &tid) {
        FCStackPop(object_struct, threadData[tid].th_state, tid);
        bool success = true;
        return success;
    }

    void initThread(const int tid) {
        threadData[tid].th_state = reinterpret_cast<FCStackThreadState *>(
            synchGetAlignedMemory(CACHE_LINE_SIZE, sizeof(FCStackThreadState)));
        FCStackThreadStateInit(object_struct, threadData[tid].th_state,
                               (int)tid);
    }

    void deinitThread(const int tid) {
        object_struct->object_struct.head=NULL;
        pthread_barrier_wait(&bar);
    }
};

#endif