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

int MAX_AGGREGATOR_THREADS;
#define NUMBER_AGGREGATORS 1
#include <ccstack_CC.h>
#include <primitives_CC.h>

#include "record_manager.h"
#define CHOOSE_AGGREGATOR(tId) (aggregator[(tId) / MAX_AGGREGATOR_THREADS])
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
    CCStackThreadState *th_state;

    ThreadData() {}

   private:
    PAD;
};

// PAD;
ThreadData threadData[MAX_THREADS_POW2];
PAD;

template <typename K, typename V>
struct alignas(PREFETCH_SIZE_BYTES) Batch {
    // PAD
    std::atomic<nodeptr> *eliminationArray;
    PAD;
    std::atomic<int> pushCounter;
    PAD;
    std::atomic<int> popCounter;
    PAD;
    std::atomic<int> finalPushCount;
    PAD;
    std::atomic<int> finalPopCount;
    // int startingPushCntr;
    // int startingPopCntr;
    PAD;
    std::atomic<bool> isBatchApplied;
    PAD;
    std::atomic_flag hasLeader;
    PAD;
    std::atomic<nodeptr> subStackTop;
    PAD;
    // struct Batch *bnext;
    // struct Batch *bprev;
    // std::atomic<struct Batch<K, V>*> bprev; //FIXME: needn't be atomic
    // std::atomic<struct Batch<K, V>*> bnext;
    ~Batch() {
        // delete[] eliminationArray;
        if (eliminationArray) free(eliminationArray);
    }
};

template <typename K, typename V>
struct alignas(PREFETCH_SIZE_BYTES) Aggregator {
    PAD;
    std::atomic<struct Batch<K, V> *> batch;
    PAD;
    // ~Aggregator() {
    //     delete batch.load();
    // }
};

template <typename K, typename V, class RecManager>
class Stack {
   private:
    Aggregator<K, V> aggregator[NUMBER_AGGREGATORS];
    PAD;
    int init[MAX_THREADS_POW2] = {
        0,
    };
    PAD;
    std::atomic<nodeptr> _top;
    PAD;
    CCStackStruct *object_struct CACHE_ALIGN;
    int64_t d1 CACHE_ALIGN, d2;

    struct Batch<K, V> *CreateNewBatch() {
        struct Batch<K, V> *newBatch;
#ifdef USE_POOLS
        if (init)
            newBatch = synchAllocObj(&pool_batch);
        else
#endif  // assert (0 && "failed");
            newBatch = new Batch<K, V>;
        // memset(newBatch, 0, sizeof(Batch<K, V>));

        newBatch->popCounter.store(0, std::memory_order_relaxed);
        newBatch->pushCounter.store(0, std::memory_order_relaxed);

        newBatch->finalPopCount.store(0, std::memory_order_relaxed);
        newBatch->finalPushCount.store(0, std::memory_order_relaxed);
        // newBatch->hasLeader.store(false, std::memory_order_relaxed);
        newBatch->hasLeader.clear(std::memory_order_relaxed);
        newBatch->isBatchApplied.store(false, std::memory_order_relaxed);
        newBatch->subStackTop.store(NULL, std::memory_order_relaxed);
        // newBatch->eliminationArray =
        //     malloc(sizeof(std::atomic<nodeptr>) * MAX_AGGREGATOR_THREADS);
        newBatch->eliminationArray = (std::atomic<nodeptr> *)malloc(
            sizeof(std::atomic<nodeptr>) * MAX_AGGREGATOR_THREADS);
        // newBatch->next = NULL;
        for (size_t i = 0; i < MAX_AGGREGATOR_THREADS; i++) {
            // newBatch->eliminationArray[i].store(NULL,
            // std::memory_order_relaxed);
            memset(newBatch->eliminationArray, 0,
                   sizeof(nodeptr) * MAX_AGGREGATOR_THREADS);
        }
        return newBatch;
    }
    void FreezeBatch(struct Aggregator<K, V> *aggregator,
                     std::atomic<struct Batch<K, V> *> batch, const int &tid) {
        std::atomic<struct Batch<K, V> *> newBatch = CreateNewBatch();
        batch.load(std::memory_order_acquire)
            ->finalPushCount.store(
                batch.load(std::memory_order_acquire)
                    ->pushCounter.load(std::memory_order_acquire),
                std::memory_order_release);

        batch.load(std::memory_order_acquire)
            ->finalPopCount.store(
                batch.load(std::memory_order_acquire)
                    ->popCounter.load(std::memory_order_acquire),
                std::memory_order_release);

        // Unlocks the waiting threads
        aggregator->batch.store(newBatch);
    }

   public:
    Stack(const int num_threads, const int _min_key, const int _max_key,
          const V _NO_VALUE, unsigned int id)
        : _top(NULL) {
        object_struct =
            synchGetAlignedMemory(S_CACHE_LINE_SIZE, sizeof(CCStackStruct));
        CCStackInit(object_struct, num_threads);
        MAX_AGGREGATOR_THREADS = ceil(float(num_threads) / NUMBER_AGGREGATORS);
        for (int i = 0; i < NUMBER_AGGREGATORS; i++) {
            struct Batch<K, V> *batch1 = CreateNewBatch();

            aggregator[i].batch = batch1;
        }
    }
    ~Stack() {}

    inline uint64_t rdtsc() {
        unsigned int hi, lo;
        __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
        return ((uint64_t)lo) | (((uint64_t)hi) << 32);
    }
    inline uint64_t hwrand() { return 200 + (rdtsc() % 100); }

    V peek(const int &tid) { return NULL; }

    bool push(const int &tid, const V &value) {
        Aggregator<K, V> *myAggregator = &CHOOSE_AGGREGATOR(tid);
        nodeptr myNode = new node_t<K, V>(0, value);
        while (true) {
            struct Batch<K, V> *myBatch = myAggregator->batch;
            int pushIndex = myBatch->pushCounter.fetch_add(1);
            myBatch->eliminationArray[pushIndex].store(myNode);
            volatile int dummy = 0;
            uint64_t backoff = hwrand();
            for (int i = 0; i < backoff; i++) {
                dummy++;
            }
            if (myBatch->hasLeader.test() == false &&
                !myBatch->hasLeader.test_and_set()) {
                FreezeBatch(myAggregator, myBatch, tid);
            } else {
                while (myBatch == myAggregator->batch) {
                }
            }

            if (pushIndex >=
                myBatch->finalPushCount.load())  // I wasn't included.
            {
                continue;  // retry in next batch which has to be your apt
                           // batch.
            }
            if (pushIndex < myBatch->finalPopCount.load(
                                std::memory_order_acquire))  // Eliminated.
            {
                return true;
            }
            CCStackPush(object_struct, threadData[tid].th_state, tid, tid);
            bool success = true;
            return success;
        }
    }

    bool pop(const int &tid) {
        Aggregator<K, V> *myAggregator = &CHOOSE_AGGREGATOR(tid);
        while (true) {
            struct Batch<K, V> *myBatch = myAggregator->batch;
            int popIndex = myBatch->popCounter.fetch_add(1);
            volatile int dummy = 0;
            uint64_t backoff = hwrand();
            for (int i = 0; i < backoff; i++) {
                dummy++;
            }
            if (myBatch->hasLeader.test() == false &&
                !myBatch->hasLeader.test_and_set()) {
                FreezeBatch(myAggregator, myBatch, tid);
            } else {
                while (myBatch == myAggregator->batch) {
                }
            }

            if (popIndex >= myBatch->finalPopCount.load())  // Not included.
            {
                continue;
            }

            if (popIndex < myBatch->finalPushCount.load(
                               std::memory_order_acquire))  // Eliminated.
            {
                while (!myBatch->eliminationArray[popIndex]
                            .load())  // Wait for Push
                                      // to write value.
                {
                }
                nodeptr my_ptr = myBatch->eliminationArray[popIndex].load();
                V returnValue = my_ptr->val;
                return returnValue;
            }
            CCStackPop(object_struct, threadData[tid].th_state, tid);
            bool success = true;
            return success;
        }
    }

    void initThread(const int tid) {
        threadData[tid].th_state = reinterpret_cast<CCStackThreadState *>(
            synchGetAlignedMemory(CACHE_LINE_SIZE, sizeof(CCStackThreadState)));
        CCStackThreadStateInit(object_struct, threadData[tid].th_state,
                               (int)tid);
    }

    void deinitThread(const int tid) {}
};

#endif