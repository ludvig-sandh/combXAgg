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

int MAX_AGGREGATOR_THREADS;
#define NUMBER_AGGREGATORS 2

#define BATCH_RETIRE
#define NODE_RETIRE

#define CHOOSE_AGGREGATOR(tId) (aggregator[(tId) / MAX_AGGREGATOR_THREADS])

#include <immintrin.h>

#include "./util/aggregatingFunnelCounter.hpp"
#include "define_global_statistics.h"
#include "pool.h"
#include "record_manager.h"
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
    PAD;
    std::atomic<node_t<K, V> *> next;
    PAD;

    node_t(K key, V val) {
        key = key;
        val = val;
        next = nullptr;
    }
    // PAD
};
#define nodeptr node_t<K, V> *

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
};

template <typename K, typename V>
struct alignas(PREFETCH_SIZE_BYTES) Aggregator {
    PAD;
    std::atomic<struct Batch<K, V> *> batch;
    PAD;
};

template <typename K, typename V, class RecMgr>
class alignas(BYTES_IN_CACHE_LINE) Stack {
   private:
    static inline thread_local std::atomic<Batch<K, V> *> newBatchPtr{nullptr};
    PAD;
    std::atomic<nodeptr> main_top;
    PAD;
    Aggregator<K, V> aggregator[NUMBER_AGGREGATORS];
    PAD;
    RecMgr * const recmgr;
    PAD;
    int init[MAX_THREADS_POW2] = {0,};
    PAD;


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
        newBatch->pushCounter.store(0,std::memory_order_relaxed);

        newBatch->finalPopCount.store(0, std::memory_order_relaxed);
        newBatch->finalPushCount.store(0, std::memory_order_relaxed);
        // newBatch->hasLeader.store(false, std::memory_order_relaxed);
        newBatch->hasLeader.clear(std::memory_order_relaxed);
        newBatch->isBatchApplied.store(false, std::memory_order_relaxed);
        newBatch->subStackTop.store(NULL, std::memory_order_relaxed);
        // newBatch->eliminationArray =
        //     malloc(sizeof(std::atomic<nodeptr>) * MAX_AGGREGATOR_THREADS);
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
        // std::atomic<struct Batch<K, V> *> newBatch;
        assert(newBatchPtr && "newBatchPtr is null");
        // newBatchPtr.load()->bprev = batch.load();
        // batch.load()->bnext = newBatchPtr;
        volatile int dummy = 0;
        for (int i = 0; i < 600; i++) {
            dummy++;
        }
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

        // newBatchPtr.load()->startingPushCntr = mypushCntr;
        // newBatchPtr.load()->startingPopCntr = mypopCntr;

        // Unlocks the waiting threads
        aggregator->batch.store(newBatchPtr);

        // long long numpush =
        //     batch.load()->finalPushCount.load(std::memory_order_relaxed) -
        //     batch.load()->startingPushCntr;
        // long long numpop =
        //     batch.load()->finalPopCount.load(std::memory_order_relaxed) -
        //     batch.load()->startingPopCntr;

        // long long num_noneliminated = (numpush - numpop) < 0
        //                                   ? (-1) * (numpush - numpop)
        //                                   : (numpush - numpop);
        // if (num_noneliminated > 50) {
        //     COUTATOMIC("dummy batch size = "
        //                << num_noneliminated << " PUSH: " << numpush
        //                << " POP: " << numpop << " STARTING PUSH COUNT "
        //                << batch.load()->startingPushCntr << " FINAL PUSH COUNT "
        //                << batch.load()->finalPushCount.load(
        //                       std::memory_order_relaxed));
        // }
        // long long num_eliminated = (numpush > numpop) ? 2 * numpop : 2 * numpush;

        // long long total_size = numpush + numpop;

        // // COUTATOMICTID("dummy batch size = " << total_size
        // // <<std::endl);
        // // GSTATS_ADD(tid, comb_batchsize, total_size);
        // GSTATS_ADD(tid, number_batches, 1);
        // GSTATS_ADD(tid, number_non_eliminated, num_noneliminated);
        // GSTATS_ADD(tid, number_eliminated, num_eliminated);

        newBatchPtr.store(CreateNewBatch(), std::memory_order_relaxed);
    }
    void CreatePushSubstackAndPush(struct Batch<K, V> *batch, int leaderIndex) {
        struct node_t<K, V> *tempTop =
            batch->eliminationArray[leaderIndex].load();
        struct node_t<K, V> *tempBot = tempTop;
        int i = 1;
        while (i < (batch->finalPushCount.load(std::memory_order_acquire) -
                    batch->finalPopCount.load(std::memory_order_acquire))) {
            while (!batch->eliminationArray[leaderIndex + i]
                        .load())  // Wait for Push to write value.
            {
            }
            nodeptr tempNode = batch->eliminationArray[leaderIndex + i].load(
                std::memory_order_relaxed);
            tempNode->next = tempTop;
            tempTop = tempNode;
            i++;
        }
        // GSTATS_ADD(0, shared_ops, 1);
        while (true) {
            struct node_t<K, V> *top = main_top;  // load top
            tempBot->next = top;
            if (main_top.compare_exchange_strong(top, tempTop)) return;
            // GSTATS_ADD(0, retry_op, 1);
        }
    }

   public:
    Stack(const int _num_threads, const int _min_key, const int _max_key,
          const V _NO_VALUE, unsigned int id)
        : main_top(NULL), recmgr (new RecMgr(_num_threads)) {

        const int tid = 0;
        initThread(tid);
        recmgr->endOp(tid);

        MAX_AGGREGATOR_THREADS = ceil(float(_num_threads) / NUMBER_AGGREGATORS);
        for (int i = 0; i < NUMBER_AGGREGATORS; i++) {
            aggregator[i].batch = CreateNewBatch();
            aggregator[i].batch.load()->finalPushCount = -1;
            aggregator[i].batch.load()->finalPopCount = -1;

            struct Batch<K, V> *batch1 = CreateNewBatch();
            // batch1->bprev = aggregator[i].batch;
            // aggregator[i].batch.load()->bnext = batch1;

            aggregator[i].batch = batch1;
        }
    }
    ~Stack() { 
        recmgr->printStatus();
        delete recmgr;
        COUTATOMIC("combxagg node size=" << sizeof(node_t<K, V>)); 
    }

    V peek(const int &tid) { return main_top ? main_top.load() : V(); }

    bool push(const int &tid, const V &value) {
        recmgr->startOp(tid);

        bool amICombiner = false;
        bool amIFreezer = false;

        Aggregator<K, V> *myAggregator = &CHOOSE_AGGREGATOR(tid);
        nodeptr myNode = new node_t<K, V>(0, value);
        while (true) {
            amICombiner = false;
            amIFreezer = false;
            struct Batch<K, V> *myBatch = myAggregator->batch;
            int pushIndex = myBatch->pushCounter.fetch_add(
                1, tid);  // Opt. Check software F&A speed up?
            myBatch->eliminationArray[pushIndex].store(myNode);

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
            {
                continue;  // retry in next batch which has to be your apt
                           // batch.
            }
            
            if (pushIndex < myBatch->finalPopCount.load(
                                std::memory_order_acquire))  // Eliminated.
            {
                // COUTATOMICTID("dummy eliminated " << value << std::endl);
                if (amIFreezer && (myBatch->finalPushCount.load(std::memory_order_acquire) ==
                    myBatch->finalPopCount.load(std::memory_order_acquire))
                ) 
                {
                    #ifdef BATCH_RETIRE
                    recmgr->retire(tid, myBatch);
                    #endif
                }
                
                recmgr->endOp(tid);
                return true;
            }

            if (pushIndex ==
                myBatch->finalPopCount.load(std::memory_order_acquire)) 
            {
                CreatePushSubstackAndPush(myBatch, pushIndex);
                myBatch->isBatchApplied.store(true, std::memory_order_release);
                amICombiner = true;
            }
            else 
            {
                while (myBatch->isBatchApplied.load() ==
                       false)  // Wait for leader to apply to main.
                {
                    // COUTATOMICTID("isBatchApplied "<<std::endl);
                }
            }

            #ifdef BATCH_RETIRE
            if (amICombiner)
                recmgr->retire(tid, myBatch);
            #endif

            recmgr->endOp(tid);

            return true;
        }
    }

    nodeptr PopFromMain(int popCount) {
        // GSTATS_ADD(0, shared_ops, 1);

        while (true) {
            nodeptr top = main_top;
            nodeptr temp = top;
            for (size_t i = 0; i < popCount; i++) {
                if (temp == NULL) break;
                temp = temp->next;
            }
            if (main_top.compare_exchange_strong(top, temp)) return top;
            // GSTATS_ADD(0, retry_op, 1);
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
        // return temp->val;
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
            if (popIndex == 0 && !myBatch->hasLeader.test_and_set()) {
                amIFreezer = true;
                FreezeBatch(myAggregator, myBatch, tid);
            }
            else 
            {
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

                nodeptr my_ptr =
                    myBatch->eliminationArray[popIndex].load();
                V returnValue = my_ptr->val;

            
                if (amIFreezer && (myBatch->finalPushCount.load(std::memory_order_acquire) ==
                    myBatch->finalPopCount.load(std::memory_order_acquire)) //FIXME: can be relaxed
                ) 
                {
                    #ifdef BATCH_RETIRE
                    recmgr->retire(tid, myBatch);
                    #endif
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
            }
            else
            {
                while (myBatch->isBatchApplied.load(
                           std::memory_order_acquire) == false) {
#ifdef USE_BACKOFF
                    // _mm_pause();
#endif
                }
            }
            // return GetRetValue(
            //     popIndex -
            //         myBatch->finalPushCount.load(std::memory_order_acquire),
            //     myBatch->subStackTop.load(std::memory_order_acquire));

            V res = GetRetValue(
            popIndex -
                myBatch->finalPushCount.load(std::memory_order_acquire),
            myBatch->subStackTop.load(std::memory_order_acquire), tid);

            #ifdef BATCH_RETIRE
            if (amICombiner)
            {
                //retire batch.
                recmgr->retire(tid, myBatch);

            }
            #endif
            recmgr->endOp(tid);
            return res;
        }
        return true;
    }

    RecMgr * debugGetRecMgr() {
        return recmgr;
    }



    void initThread(const int tid) {

    newBatchPtr.store(CreateNewBatch(), std::memory_order_relaxed);

    if (init[tid]) return;
    else init[tid] = !init[tid];
    recmgr->initThread(tid);


#ifdef USE_POOLS
        if (!init) {
            synchInitPool(&pool_batch, sizeof(Batch<K, V>));
            synchInitPool(&pool_node, sizeof(node_t<K, V>));

            init = true;
        }
#endif
        if (0 == tid)
            COUTATOMICTID("comxagg batch size: " << sizeof(Batch<K, V>)
                                                 << std::endl);
}

    void deinitThread(const int tid) {
        if (!init[tid]) return;
        else init[tid] = !init[tid];
        recmgr->deinitThread(tid);
    }
};

#endif