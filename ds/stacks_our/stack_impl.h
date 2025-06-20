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

// FIXME: creating unnecessary aggregators/
// FIXME: memory leaks

#define MAX_AGGREGATOR_THREADS 48
#define NUMBER_AGGREGATORS 64

#define CHOOSE_AGGREGATOR(tId) (aggregator[(tId) / MAX_AGGREGATOR_THREADS])

#include <immintrin.h>
#include "./util/aggregatingFunnelCounter.hpp"
#include "pool.h"
#include "record_manager.h"

#include "define_global_statistics.h"
// static __thread SynchPoolStruct pool_node CACHE_ALIGN;
static __thread SynchPoolStruct pool_batch CACHE_ALIGN;
static __thread bool init = false;

template <typename K, typename V>
class alignas(BYTES_IN_CACHE_LINE) node_t {
   public:
    K key;
    V val;
    // PAD
    std::atomic<node_t<K, V> *> next;
    // PAD

    node_t(K key, V val) {
        key = key;
        val = val;
        next = nullptr;
    }
    // PAD
};
#define nodeptr node_t<K, V> *

template <typename K, typename V>
struct alignas(BYTES_IN_CACHE_LINE) Batch {
    // PAD
    std::atomic<nodeptr> eliminationArray[MAX_AGGREGATOR_THREADS];
    // PAD
    #ifdef USE_AF
        SIMPLE_AGG_FUNNEL::AggFunnelCounter<int> pushCounter;
        SIMPLE_AGG_FUNNEL::AggFunnelCounter<int> popCounter;
    #else    
        std::atomic<int> pushCounter;
        std::atomic<int> popCounter;
    #endif
    // PAD

    // PAD
    std::atomic<int> finalPushCount;
    // PAD
    std::atomic<int> finalPopCount;
    // PAD
    std::atomic<bool> isBatchApplied;
    // PAD
    std::atomic_flag hasLeader;
    std::atomic<nodeptr> subStackTop;
    // //PAD
    std::atomic<nodeptr> subStackBot;
    // PAD
    //  struct Batch *next;
};

template <typename K, typename V>
struct alignas(BYTES_IN_CACHE_LINE) Aggregator {
    // PAD
    std::atomic<struct Batch<K, V> *> batch;
    // PAD
};

template <typename K, typename V, class RecManager>
class alignas(BYTES_IN_CACHE_LINE) Stack {
   private:
    // PAD
    std::atomic<nodeptr> main_top;
    // PAD
    Aggregator<K, V> aggregator[NUMBER_AGGREGATORS];
    // PAD

    struct Batch<K, V> *CreateNewBatch() {
        struct Batch<K, V> *newBatch;
        if (init)
            newBatch = synchAllocObj(&pool_batch);
        else
            newBatch = new Batch<K, V>;
        // memset(newBatch, 0, sizeof(Batch<K, V>));

        newBatch->popCounter.store(0, std::memory_order_relaxed);
        newBatch->pushCounter.store(0,std::memory_order_relaxed);
        newBatch->finalPopCount.store(0, std::memory_order_relaxed);
        newBatch->finalPushCount.store(0, std::memory_order_relaxed);
        // newBatch->hasLeader.store(false, std::memory_order_relaxed);
        newBatch->hasLeader.clear(std::memory_order_relaxed);
        newBatch->isBatchApplied.store(false, std::memory_order_relaxed);
        newBatch->subStackBot.store(NULL, std::memory_order_relaxed);
        newBatch->subStackTop.store(NULL, std::memory_order_relaxed);

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
                     std::atomic<struct Batch<K, V> *> batch) {
        std::atomic<struct Batch<K, V> *> newBatch;
        newBatch.store(CreateNewBatch(),
                       std::memory_order_relaxed);
        // newBatch.load(std::memory_order_acquire)->next =
        // batch.load(std::memory_order_acquire); Snapshots the counters
        #ifdef USE_AF
        batch.load(std::memory_order_acquire)
            ->finalPushCount.store(
                batch.load(std::memory_order_acquire)->pushCounter.load(),
                std::memory_order_release);


        batch.load(std::memory_order_acquire)
            ->finalPopCount.store(
                batch.load(std::memory_order_acquire)
                    ->popCounter.load(),
                std::memory_order_release);
        #else
        batch.load(std::memory_order_acquire)
            ->finalPushCount.store(
                batch.load(std::memory_order_acquire)->pushCounter.load(std::memory_order_acquire),
                std::memory_order_release);


        batch.load(std::memory_order_acquire)
            ->finalPopCount.store(
                batch.load(std::memory_order_acquire)
                    ->popCounter.load(std::memory_order_acquire),
                std::memory_order_release);

        #endif

        // Unlocks the waiting threads
        aggregator->batch.store(newBatch, std::memory_order_release);
    }
    void PushToMain(
        nodeptr subStackTop,
        nodeptr subStackBot)  // Aggregation of multiple push operations
                              // to main(same as aggregation of F&A).
    {
        while (true) {
            struct node_t<K, V> *top = main_top;  // load top
            subStackBot->next = top;
            if (main_top.compare_exchange_strong(top, subStackTop)) return;
        }
    }
    void CreatePushSubstack(struct Batch<K, V> *batch, int leaderIndex) {
        batch->subStackBot.store(batch->eliminationArray[leaderIndex].load(
                                     std::memory_order_relaxed),
                                 std::memory_order_relaxed);
        struct node_t<K, V> *tempTop =
            batch->subStackBot.load(std::memory_order_relaxed);
        int i = 1;
        while (leaderIndex + i < batch->finalPushCount) {
            while (!batch->eliminationArray[leaderIndex + i].load(
                std::memory_order_acquire))  // Wait for Push to write value.
            {
                _mm_pause();
            }
            nodeptr tempNode = batch->eliminationArray[leaderIndex + i].load(
                std::memory_order_relaxed);
            tempNode->next = tempTop;
            tempTop = tempNode;
            i++;
        }
        batch->subStackTop = tempTop;
        return;
    }

   public:
    Stack(const int num_threads, const int _min_key, const int _max_key,
          const V _NO_VALUE, unsigned int id)
        : main_top(NULL) {
        for (int i = 0; i < NUMBER_AGGREGATORS; i++) {
            aggregator[i].batch = CreateNewBatch();
        }
    }
    ~Stack() {}

    V peek(const int &tid) {
        return main_top ? main_top.load(std::memory_order_acquire) : V();
    }

    bool push(const int &tid, const V &value) {
        if (!init) {
            // synchInitPool(&pool_node, sizeof(node_t<K, V>));
            synchInitPool(&pool_batch, sizeof(Batch<K, V>));
            init = true;
        }
        Aggregator<K, V> *myAggregator = &CHOOSE_AGGREGATOR(tid);
        nodeptr myNode = new node_t<K, V>(0, value);
        // nodeptr myNode = synchAllocObj(&pool_node);
        while (true) {
            struct Batch<K, V> *myBatch = myAggregator->batch;
            int pushIndex = myBatch->pushCounter.fetch_add(
                1, tid);  // Opt. Check software F&A speed up?
                          // myBatch->eliminationArray[pushIndex] = myNode;

            // COUTATOMICTID("dummy pushing " << value << std::endl);
            if (pushIndex == 0 &&
                !myBatch->hasLeader.test_and_set())  // Should be test and set.
            {
                FreezeBatch(myAggregator, myBatch);
                int numpush = myBatch.load(std::memory_order_acquire)->finalPushCount.load();
                int numpop = myBatch.load(std::memory_order_acquire)->finalPopCount.load();

                int num_noneliminated = (numpush-numpop) < 0 ? (-1)*(numpush-numpop) : (numpush-numpop);

                int num_eliminated = (numpush > numpop) ? numpop : numpush;

                int total_size = num_eliminated + num_noneliminated;

                GSTATS(tid, comb_batchsize, total_size);
                // GSTATS(tid, comb_numbatchpop, numpop);

            } else {
                while (
                    myBatch ==
                    myAggregator->batch)  // Spin until freezing has finished.
                {
                _mm_pause();
                }
            }
            if (pushIndex >=
                myBatch->finalPushCount.load(
                    std::memory_order_acquire))  // I wasn't included.
                continue;                        // Try to join a new batch

            myBatch->eliminationArray[pushIndex].store(
                myNode, std::memory_order_release);
            if (pushIndex < myBatch->finalPopCount.load(
                                std::memory_order_acquire))  // Eliminated.
            {
                // COUTATOMICTID("dummy eliminated " << value << std::endl);
                return true;
            }
            if (pushIndex ==
                myBatch->finalPopCount.load(std::memory_order_acquire)) {
                CreatePushSubstack(myBatch, pushIndex);
                PushToMain(
                    myBatch->subStackTop,
                    myBatch->subStackBot.load(std::memory_order_relaxed));
                myBatch->isBatchApplied.store(true, std::memory_order_release);
            } else {
                while (
                    myBatch->isBatchApplied.load(std::memory_order_acquire) ==
                    false)  // Wait for leader to apply to main.
                {
                _mm_pause();

                }
            }

            return true;
        }
    }

    nodeptr PopFromMain(int popCount) {
        while (true) {
            nodeptr top = main_top;
            nodeptr temp = top;
            for (size_t i = 0; i < popCount; i++) {
                if (temp == NULL) break;
                temp = temp->next;
            }
            if (main_top.compare_exchange_strong(top, temp)) return top;
        }
    }

    V GetRetValue(int index, nodeptr top) {
        if (top == NULL) {
            return V();
        }
        nodeptr temp = top;
        for (size_t i = 0; i < index; i++) {
            temp = temp->next;
            if (temp == NULL) {
                return V();
            }
        }
        return temp->val;
    }

    bool pop(const int &tid) {
        if (!init) {
            // synchInitPool(&pool_node, sizeof(node_t<K, V>));
            synchInitPool(&pool_batch, sizeof(Batch<K, V>));
            init = true;
        }
        Aggregator<K, V> *myAggregator = &CHOOSE_AGGREGATOR(tid);
        bool success = false;
        while (true) {
            struct Batch<K, V> *myBatch = myAggregator->batch;
            int popIndex = myBatch->popCounter.fetch_add(1, tid);
            // COUTATOMICTID("DUMMY popping " << std::endl);
            if (popIndex == 0 && !myBatch->hasLeader.test_and_set()) {
                FreezeBatch(myAggregator, myBatch);
            } else {
                while (myBatch == myAggregator->batch) {
                    _mm_pause();
                }
            }
            if (popIndex >= myBatch->finalPopCount.load(
                                std::memory_order_acquire))  // Not included.
                continue;
            if (popIndex < myBatch->finalPushCount.load(
                               std::memory_order_acquire))  // Eliminated.
            {
                // COUTATOMICTID("DUMMY elimin pop , popIndex = "
                //   << popIndex << " , Push count = "
                //   << myBatch->finalPushCount.load(std::memory_order_acquire)
                //   << " FinalPopCount = "
                //   << myBatch->finalPopCount.load(std::memory_order_acquire)
                //   << std::endl);
                while (!myBatch->eliminationArray[popIndex].load(
                    std::memory_order_acquire))  // Wait for Push
                                                 // to write value.
                {
                    _mm_pause();
                }
                nodeptr my_ptr = myBatch->eliminationArray[popIndex].load(
                    std::memory_order_acquire);
                V returnValue = my_ptr->val;
                // synchRecycleObj(&pool_node, my_ptr);
                return returnValue;
            }
            if (popIndex ==
                myBatch->finalPushCount.load(std::memory_order_acquire)) {
                int remainingPops =
                    myBatch->finalPopCount.load(std::memory_order_acquire) -
                    myBatch->finalPushCount.load(std::memory_order_acquire);
                myBatch->subStackTop.store(PopFromMain(remainingPops),
                                           std::memory_order_release);
                myBatch->isBatchApplied.store(true, std::memory_order_release);
            } else {
                while (myBatch->isBatchApplied.load(
                           std::memory_order_acquire) == false) {
                    _mm_pause();
                }
            }
            return GetRetValue(
                popIndex -
                    myBatch->finalPushCount.load(std::memory_order_acquire),
                myBatch->subStackTop.load(std::memory_order_acquire));
        }
        return success;
    }
};

#endif