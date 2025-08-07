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

#include <hstack.h>
#include <primitives.h>

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
    HStackThreadState *th_state;

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
    HStackStruct *object_struct CACHE_ALIGN;
    int64_t d1 CACHE_ALIGN, d2;
    pthread_barrier_t bar;
    const int num_thread;

   public:
    Stack(const int num_threads, const int _min_key, const int _max_key,
          const V _NO_VALUE, unsigned int id)
        : _top(NULL), num_thread(num_threads) {
        object_struct =
            synchGetAlignedMemory(S_CACHE_LINE_SIZE, sizeof(HStackStruct));
        HStackInit(object_struct, num_threads, HSYNCH_DEFAULT_NUMA_POLICY);
        pthread_barrier_init(&bar, NULL, num_threads);

        COUTATOMIC("Stack object initialized with "
                   << num_thread << " threads and "
                   << HSYNCH_DEFAULT_NUMA_POLICY << " NUMA policy."
                   << std::endl);
    }
    ~Stack() {}

    V peek(const int &tid) { return NULL; }

    bool push(const int &tid, const V &value) {
        VERBOSE COUTATOMICTID("pushing " << std::endl);
        HStackPush(object_struct, threadData[tid].th_state, tid, tid);
        VERBOSE COUTATOMICTID("pushed " << std::endl);

        return true;
    }

    bool pop(const int &tid) {
        VERBOSE COUTATOMICTID("popping " << std::endl);

        HStackPop(object_struct, threadData[tid].th_state, tid);
        VERBOSE COUTATOMICTID("popped " << std::endl);

        bool success = true;
        // COUTATOMICTID("DUMMY popping " << std::endl);
        return success;
    }

    void initThread(const int tid) {
        // if (init[tid]) return;
        // else init[tid] = !init[tid];
        // recmgr->initThread(tid);
        // COUTATOMICTID("Initializing thread " << tid << std::endl);

        threadData[tid].th_state = reinterpret_cast<HStackThreadState *>(
            synchGetAlignedMemory(CACHE_LINE_SIZE, sizeof(HStackThreadState)));

        HStackThreadStateInit(object_struct, threadData[tid].th_state,
                              (int)tid);
    }

    void deinitThread(const int tid) {
        VERBOSE COUTATOMICTID("Deinitializing thread " << tid << std::endl);
        if (tid == 0)
            HSynchStructInit(&object_struct->object_struct, num_thread,
                             HSYNCH_DEFAULT_NUMA_POLICY);
        pthread_barrier_wait(&bar);
        // if (!init[tid]) return;
        // else init[tid] = !init[tid];
        // // recmgr->deinitThread(tid);
    }
};

#endif