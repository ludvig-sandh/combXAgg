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
#include <cmath>
// FIXME: memory leaks

int MAX_AGGREGATOR_THREADS;
#define NUMBER_AGGREGATORS 2

#define CHOOSE_AGGREGATOR(tId) (aggregator[(tId) / MAX_AGGREGATOR_THREADS])

#include <immintrin.h>

#include "concprimitives.h"

#include "./util/aggregatingFunnelCounter.hpp"
#include "define_global_statistics.h"
#include "pool.h"
#include "record_manager.h"
static __thread bool init = false;

template <typename K, typename V>
class alignas(BYTES_IN_CACHE_LINE) node_t {
   public:
    K key;
    V val;
    std::atomic<node_t<K, V> *> next;
    node_t(K key, V val) {
        key = key;
        val = val;
        next = nullptr;
    }
};
#define nodeptr node_t<K, V> *

template <typename K, typename V>
struct alignas(BYTES_IN_CACHE_LINE) Batch {
    // std::atomic<nodeptr> *eliminationArray;
    paddedAtomic<nodeptr> *eliminationArray;
    std::atomic<int> pushCounter;
    std::atomic<int> popCounter;
    std::atomic<int> finalPushCount;
    std::atomic<int> finalPopCount;
    std::atomic<bool> isBatchApplied;
    std::atomic_flag hasLeader;
    std::atomic<nodeptr> subStackTop;
    std::atomic<nodeptr> subStackBot;
};

template <typename K, typename V>
struct alignas(BYTES_IN_CACHE_LINE) Aggregator {
    std::atomic<struct Batch<K, V> *> batch;
};

template <typename K, typename V, class RecMgr>
class alignas(BYTES_IN_CACHE_LINE) Stack {
   private:
    std::atomic<nodeptr> main_top;
    Aggregator<K, V> aggregator[NUMBER_AGGREGATORS];
    PAD;
    RecMgr *const recmgr;
    PAD;
    int init[MAX_THREADS_POW2] = {
        0,
    };
    PAD;

    struct Batch<K, V> *CreateNewBatch() {
        struct Batch<K, V> *newBatch;
        newBatch = new Batch<K, V>;

        newBatch->popCounter.store(0, std::memory_order_seq_cst);
        newBatch->pushCounter.store(0, std::memory_order_seq_cst);
        newBatch->finalPopCount.store(0, std::memory_order_seq_cst);
        newBatch->finalPushCount.store(0, std::memory_order_seq_cst);
        newBatch->hasLeader.clear(std::memory_order_seq_cst);
        newBatch->isBatchApplied.store(false, std::memory_order_seq_cst);
        newBatch->subStackBot.store(NULL, std::memory_order_seq_cst);
        newBatch->subStackTop.store(NULL, std::memory_order_seq_cst);
        // newBatch->eliminationArray = (std::atomic<nodeptr> *)malloc(
        //     sizeof(std::atomic<nodeptr>) * MAX_AGGREGATOR_THREADS);
        // for (size_t i = 0; i < MAX_AGGREGATOR_THREADS; i++) {
        //     memset(newBatch->eliminationArray, 0,
        //            sizeof(nodeptr) * MAX_AGGREGATOR_THREADS);
        // }
        newBatch->eliminationArray = (paddedAtomic<nodeptr> *)malloc(
            sizeof(paddedAtomic<nodeptr>) * MAX_AGGREGATOR_THREADS);
        for (size_t i = 0; i < MAX_AGGREGATOR_THREADS; i++)
        {
            // newBatch->eliminationArray[i].store(NULL, std::memory_order_relaxed);
            //set each slots nodeptr to null
            newBatch->eliminationArray[i].ui = NULL;

        }
                   
        return newBatch;
    }

    void FreezeBatch(struct Aggregator<K, V> *aggregator,
                     std::atomic<struct Batch<K, V> *> batch, const int &tid) {
        std::atomic<struct Batch<K, V> *> newBatch;
        newBatch.store(CreateNewBatch(), std::memory_order_seq_cst);
        batch.load()->finalPushCount.store(batch.load()->pushCounter.load());
        batch.load()->finalPopCount.store(batch.load()->popCounter.load());
#endif

        // Unlocks the waiting threads
        aggregator->batch.store(newBatch);
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
        // batch->subStackBot.store(batch->eliminationArray[leaderIndex].load());
        batch->subStackBot.store(batch->eliminationArray[leaderIndex].ui);

        struct node_t<K, V> *tempTop = batch->subStackBot.load();
        int i = 1;
        while (leaderIndex + i < batch->finalPushCount) {
            // while (!batch->eliminationArray[leaderIndex + i].load())  // Wait for Push to write value.
            while (!batch->eliminationArray[leaderIndex + i].ui)  // Wait for Push to write value.
            {
#ifdef USE_BACKOFF
                // _mm_pause();
#endif
            }
            // nodeptr tempNode = batch->eliminationArray[leaderIndex + i].load();
            nodeptr tempNode = batch->eliminationArray[leaderIndex + i].ui;

            tempNode->next = tempTop;
            tempTop = tempNode;
            i++;
        }
        batch->subStackTop = tempTop;
        return;
    }

   public:
    Stack(const int _num_threads, const int _min_key, const int _max_key,
          const V _NO_VALUE, unsigned int id)
        : main_top(NULL), recmgr(new RecMgr(_num_threads)) {
        const int tid = 0;
        initThread(tid);
        recmgr->endOp(tid);

        MAX_AGGREGATOR_THREADS = ceil(float(_num_threads) / NUMBER_AGGREGATORS);
        for (int i = 0; i < NUMBER_AGGREGATORS; i++) {
            aggregator[i].batch = CreateNewBatch();
        }
    }
    ~Stack() {
        recmgr->printStatus();
        delete recmgr;
    }

    V peek(const int &tid) { return main_top ? main_top.load() : V(); }

    bool push(const int &tid, const V &value) {
        recmgr->startOp(tid);
        bool amICombiner;
        bool amIFreezer;
        Aggregator<K, V> *myAggregator = &CHOOSE_AGGREGATOR(tid);
        nodeptr myNode = new node_t<K, V>(0, value);
        while (true) {
            amICombiner = false;
            amIFreezer = false;
            struct Batch<K, V> *myBatch = myAggregator->batch;
            int pushIndex = myBatch->pushCounter.fetch_add(
                1);

            if (pushIndex == 0 &&
                !myBatch->hasLeader.test_and_set())  // Should be test and set.
            {
                amIFreezer = true;
                FreezeBatch(myAggregator, myBatch, tid);
            } else {
                while (
                    myBatch ==
                    myAggregator->batch)  // Spin until freezing has finished.
                {
                }
            }
            if (pushIndex >=
                myBatch->finalPushCount.load())  // I wasn't included.
                continue;                        // Try to join a new batch

            // myBatch->eliminationArray[pushIndex].store(myNode);
            myBatch->eliminationArray[pushIndex].ui = myNode;

            if (pushIndex < myBatch->finalPopCount.load())  // Eliminated.
            {
                // check if everyone got eliminated then freezer must retire
                // batch.
                if (amIFreezer && (myBatch->finalPushCount.load() ==
                                   myBatch->finalPopCount.load())) {
                    // recmgr->retire(tid, myBatch);
                }
                recmgr->endOp(tid);
                return true;

                // FIXME retire: the case when all ops in batch get
                // eliminated. Who retires? the batch. May be the freezer
                // should be the owner. And check if all ops wll be
                // eliminated then should retire the batch. Else leak
                // occurs.
            }

            if (pushIndex == myBatch->finalPopCount.load()) {
                CreatePushSubstack(myBatch, pushIndex);
                PushToMain(myBatch->subStackTop, myBatch->subStackBot.load());
                myBatch->isBatchApplied.store(true);
                amICombiner = true;
            } else {
                while (
                    myBatch->isBatchApplied.load(std::memory_order_acquire) ==
                    false)  // Wait for leader to apply to main.
                {
                }
            }
            // if (amICombiner) recmgr->retire(tid, myBatch);
            recmgr->endOp(tid);

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

    V GetRetValue(int index, nodeptr top, const int &tid) {
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
        V res = temp->val;
        recmgr->retire(tid, temp);
        return res;
    }

    bool pop(const int &tid) {
        bool amICombiner;
        bool amIFreezer;
        recmgr->startOp(tid);
        Aggregator<K, V> *myAggregator = &CHOOSE_AGGREGATOR(tid);
        bool success = false;
        while (true) {
            amICombiner = false;
            amIFreezer = false;
            struct Batch<K, V> *myBatch = myAggregator->batch;
            int popIndex = myBatch->popCounter.fetch_add(1);
            if (popIndex == 0 && !myBatch->hasLeader.test_and_set()) {
                FreezeBatch(myAggregator, myBatch, tid);
                amIFreezer = true;
            } else 
            {
                while (myBatch == myAggregator->batch) {
#ifdef USE_BACKOFF
                    _mm_pause();
#endif
                }
            }

            if (popIndex >= myBatch->finalPopCount.load())  // Not included.
                continue;

            if (popIndex < myBatch->finalPushCount.load())  // Eliminated.
            {
                while (!myBatch->eliminationArray[popIndex].ui)  // Wait for Push to write value.
                {
                }
                // nodeptr my_ptr = myBatch->eliminationArray[popIndex].load();
                nodeptr my_ptr = myBatch->eliminationArray[popIndex].ui;

                V returnValue = my_ptr->val;
            
                if (amIFreezer && (myBatch->finalPushCount.load(std::memory_order_acquire) ==
                    myBatch->finalPopCount.load(std::memory_order_acquire)) //FIXME: can be relaxed
                ) 
                {
                    // recmgr->retire(tid, myBatch);
                }
            
            
                recmgr->endOp(tid);
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
                amICombiner = true;
            } else {
                while (myBatch->isBatchApplied.load() == false) {
                }
            }

            V res = GetRetValue(popIndex - myBatch->finalPushCount.load(),
                                myBatch->subStackTop.load(), tid);
            // if (amICombiner) recmgr->retire(tid, myBatch);
            recmgr->endOp(tid);
            return res;
        }
        // deadpath
        return success;
    }

    RecMgr *debugGetRecMgr() { return recmgr; }

    void initThread(const int tid) {
        if (init[tid])
            return;
        else
            init[tid] = !init[tid];
        recmgr->initThread(tid);
    }

    void deinitThread(const int tid) {
        if (!init[tid])
            return;
        else
            init[tid] = !init[tid];
        recmgr->deinitThread(tid);
    }
};
