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
// FIXME: creating unnecessary aggregators/
// FIXME: memory leaks

int MAX_AGGREGATOR_THREADS;
#define NUMBER_AGGREGATORS 4

#define CHOOSE_AGGREGATOR(tId) (aggregator[(tId) / MAX_AGGREGATOR_THREADS])

#include <immintrin.h>
#include "./util/aggregatingFunnelCounter.hpp"
#include "pool.h"
#include "record_manager.h"

#include "define_global_statistics.h"
static __thread SynchPoolStruct pool_node CACHE_ALIGN;
static __thread SynchPoolStruct pool_batch CACHE_ALIGN;
static __thread bool init = false;

// #define USE_POOLS
// #define USE_BACKOFF // doesn't help us.

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
    std::atomic<nodeptr> *eliminationArray;
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

template <typename K, typename V, class RecMgr>
class alignas(BYTES_IN_CACHE_LINE) Stack {
   private:
    // PAD
    std::atomic<nodeptr> main_top;
    // PAD
    Aggregator<K, V> aggregator[NUMBER_AGGREGATORS];
    PAD;
    RecMgr * const recmgr;
    PAD;
    int init[MAX_THREADS_POW2] = {0,};
    PAD;

    struct Batch<K, V> *CreateNewBatch() {
        struct Batch<K, V> *newBatch;
// #ifdef USE_POOLS
//         if (init)
//             newBatch = synchAllocObj(&pool_batch);
//         else
// #endif            // assert (0 && "failed");
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
        newBatch->eliminationArray = (std::atomic<nodeptr> *)malloc(sizeof(std::atomic<nodeptr>)*MAX_AGGREGATOR_THREADS);
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
        // GSTATS_ADD(0, comb_numshared, 1);
        while (true) {
            // GSTATS_ADD(0, comb_numretrytop, 1);
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
#ifdef USE_BACKOFF
                    // _mm_pause();
#endif
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
    Stack(const int _num_threads, const int _min_key, const int _max_key,
          const V _NO_VALUE, unsigned int id)
        : main_top(NULL), recmgr (new RecMgr(_num_threads)) {
            
            const int tid = 0;
            initThread(tid);
            recmgr->endOp(tid);

            MAX_AGGREGATOR_THREADS = ceil(float(_num_threads)/NUMBER_AGGREGATORS);
        for (int i = 0; i < NUMBER_AGGREGATORS; i++) {
            aggregator[i].batch = CreateNewBatch();
        }
    }
    ~Stack() {
        COUTATOMIC("combxagg node size=" << sizeof(node_t<K, V>));
        recmgr->printStatus();
        delete recmgr;
    }

    V peek(const int &tid) {
        return main_top ? main_top.load(std::memory_order_acquire) : V();
    }

    bool push(const int &tid, const V &value) {
        recmgr->startOp(tid);
        bool amICombiner = false;
        bool amIFreezer = false;

        Aggregator<K, V> *myAggregator = &CHOOSE_AGGREGATOR(tid);
// #ifdef USE_POOL
// nodeptr myNode = synchAllocObj(&pool_node);
// myNode->key = 0;
// myNode->val = 0;
// #else
nodeptr myNode = new node_t<K, V>(0, value);
// #endif
        while (true) {
            amICombiner = false;
            amIFreezer = false;
            struct Batch<K, V> *myBatch = myAggregator->batch;
            int pushIndex = myBatch->pushCounter.fetch_add(
                1, tid);  // Opt. Check software F&A speed up?
                          // myBatch->eliminationArray[pushIndex] = myNode;

            // COUTATOMICTID("dummy pushing " << value << std::endl);
            if (pushIndex == 0 &&
                !myBatch->hasLeader.test_and_set())  // Should be test and set.
            {
                amIFreezer = true;
                FreezeBatch(myAggregator, myBatch, tid);
                // int numpush = myBatch->finalPushCount.load(std::memory_order_relaxed);
                // int numpop = myBatch->finalPopCount.load(std::memory_order_relaxed);

                // // int num_noneliminated = (numpush-numpop) < 0 ? (-1)*(numpush-numpop) : (numpush-numpop);

                // // int num_eliminated = (numpush > numpop) ? numpop : numpush;

                // int total_size = numpush + numpop;

                // // COUTATOMICTID("dummy batch size = " << total_size <<std::endl);
                // GSTATS_APPEND(tid, comb_batchsize, total_size);
                // GSTATS_ADD(tid, comb_numbatch, 1);
                // GSTATS(tid, comb_numbatchpop, numpop);

            } else {
                while (
                    myBatch ==
                    myAggregator->batch)  // Spin until freezing has finished.
                {
                #ifdef USE_BACKOFF
                    _mm_pause();
#endif
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
                            //check if everyone got eliminated then freezer must retire batch.
                if (amIFreezer && (myBatch->finalPushCount.load(std::memory_order_acquire) ==
                    myBatch->finalPopCount.load(std::memory_order_acquire))
                ) 
                {
                    recmgr->retire(tid, myBatch);
                }
                    
                
                recmgr->endOp(tid);
                return true;

                // FIXME retire: the case when all ops in batch get eliminated. Who retires? the batch. May be the freezer should be the owner. And check if all ops wll be eliminated then should retire the batch. Else leak occurs.
            }

            if (pushIndex ==
                myBatch->finalPopCount.load(std::memory_order_acquire)) {
                CreatePushSubstack(myBatch, pushIndex);
                PushToMain(
                    myBatch->subStackTop,
                    myBatch->subStackBot.load(std::memory_order_relaxed));
                myBatch->isBatchApplied.store(true, std::memory_order_release);

                amICombiner = true;
            }
            else 
            {
                while (
                    myBatch->isBatchApplied.load(std::memory_order_acquire) ==
                    false)  // Wait for leader to apply to main.
                {
                #ifdef USE_BACKOFF
                    // _mm_pause();
                #endif
                }
            }

            if (amICombiner)
                recmgr->retire(tid, myBatch);
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
        recmgr->startOp(tid);

        bool amICombiner = false;
        bool amIFreezer = false;
        Aggregator<K, V> *myAggregator = &CHOOSE_AGGREGATOR(tid);
        bool success = false;
        while (true) {
            amICombiner = false;
            amIFreezer = false;
            struct Batch<K, V> *myBatch = myAggregator->batch;
            int popIndex = myBatch->popCounter.fetch_add(1, tid);
            // COUTATOMICTID("DUMMY popping " << std::endl);
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
                    #ifdef USE_BACKOFF
                    // _mm_pause();
                    #endif
                }
                nodeptr my_ptr = myBatch->eliminationArray[popIndex].load(
                    std::memory_order_acquire);
                V returnValue = my_ptr->val;
            
                if (amIFreezer && (myBatch->finalPushCount.load(std::memory_order_acquire) ==
                    myBatch->finalPopCount.load(std::memory_order_acquire)) //FIXME: can be relaxed
                ) 
                {
                    recmgr->retire(tid, myBatch);
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
                while (myBatch->isBatchApplied.load(
                           std::memory_order_acquire) == false) {
#ifdef USE_BACKOFF
                    // _mm_pause();
#endif
                }
            }

            V res = GetRetValue(
                popIndex -
                    myBatch->finalPushCount.load(std::memory_order_acquire),
                myBatch->subStackTop.load(std::memory_order_acquire), tid);

            if (amICombiner)
            {
                //retire batch.
                recmgr->retire(tid, myBatch);

            }
            recmgr->endOp(tid);
            return res;
        }

        //deadpath
        recmgr->endOp(tid);
        return success;
    }

    RecMgr * debugGetRecMgr() {
        return recmgr;
    }

    void initThread(const int tid) {
        if (init[tid]) return;
        else init[tid] = !init[tid];
        recmgr->initThread(tid);  

// #ifdef USE_POOLS
//         if (!init) {
//             synchInitPool(&pool_batch, sizeof(Batch<K, V>));
//             synchInitPool(&pool_node, sizeof(node_t<K, V>));
            
//             init = true;
//         }
// #endif
        if (0 == tid) COUTATOMICTID("comxagg batch size: " << sizeof(Batch<K, V>) << std::endl);
    }

    void deinitThread(const int tid) {
        if (!init[tid]) return;
        else init[tid] = !init[tid];
        recmgr->deinitThread(tid);
    }


};

#endif