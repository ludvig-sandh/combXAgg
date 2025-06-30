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
#define NUMBER_AGGREGATORS 8 //cant have more than 8 agg for 192 threads with 48 max threads per agg

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
    std::atomic<nodeptr> eliminationArray[MAX_AGGREGATOR_THREADS];
    // PAD
    std::atomic<int> finalPushCount;
    // PAD
    std::atomic<int> finalPopCount;
    int startingPushCntr;
    int startingPopCntr;
    // PAD
    std::atomic<bool> isBatchApplied;
    // PAD
    std::atomic_flag hasLeader;
    std::atomic<nodeptr> subStackTop;
    // //PAD
    std::atomic<nodeptr> subStackBot;
    // PAD
     struct Batch *bnext;
     struct Batch *bprev;
    // std::atomic<struct Batch<K, V>*> bprev; //FIXME: needn't be atomic
    // std::atomic<struct Batch<K, V>*> bnext;
};

template <typename K, typename V>
struct alignas(BYTES_IN_CACHE_LINE) Aggregator {
    PAD;
    std::atomic<int> pushCounter;
    PAD;
    std::atomic<int> popCounter;
    PAD;
    std::atomic<struct Batch<K, V> *> batch;
    PAD;
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
#ifdef USE_POOLS
        if (init)
            newBatch = synchAllocObj(&pool_batch);
        else
#endif            // assert (0 && "failed");
        newBatch = new Batch<K, V>;
        // memset(newBatch, 0, sizeof(Batch<K, V>));

        // newBatch->popCounter.store(0, std::memory_order_relaxed);
        // newBatch->pushCounter.store(0,std::memory_order_relaxed);
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
 
        newBatch.store(CreateNewBatch(), std::memory_order_relaxed);
       
        newBatch.load()->bprev = batch.load();
        batch.load()->bnext = newBatch;
        
        int mypushCntr = aggregator->pushCounter;
        int mypopCntr = aggregator->popCounter;

        batch.load()->finalPushCount.store(mypushCntr);
        batch.load()->finalPopCount.store(mypopCntr);

        newBatch.load()->startingPushCntr = mypushCntr;
        newBatch.load()->startingPopCntr = mypopCntr; 


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
        batch->subStackBot.store(batch->eliminationArray[leaderIndex].load());
        struct node_t<K, V> *tempTop = batch->subStackBot.load(std::memory_order_relaxed);

        int i = 1;
        while (i < ((batch->finalPushCount - batch->startingPushCntr) - (batch->finalPopCount - batch->startingPopCntr)) ) 
        {
            while (!batch->eliminationArray[leaderIndex + i].load(
                ))  // Wait for Push to write value.
            {
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
            aggregator[i].batch.load()->finalPushCount = -1;
            aggregator[i].batch.load()->finalPopCount = -1;

            struct Batch<K, V>* batch1 = CreateNewBatch();
            batch1->bprev = aggregator[i].batch;
            aggregator[i].batch.load()->bnext =batch1;

            aggregator[i].batch = batch1;
            aggregator[i].pushCounter.store(0,std::memory_order_relaxed);
            aggregator[i].popCounter.store(0, std::memory_order_relaxed);
        }
    }
    ~Stack() {
        COUTATOMIC("combxagg node size=" << sizeof(node_t<K, V>));
    }

    V peek(const int &tid) {
        return main_top ? main_top.load() : V();
    }

    bool push(const int &tid, const V &value) {
        Aggregator<K, V> *myAggregator = &CHOOSE_AGGREGATOR(tid);
        nodeptr myNode = new node_t<K, V>(0, value);
        
        int pushIndex = myAggregator->pushCounter.fetch_add(1, tid);
        struct Batch<K, V> *mayBeMyBatch = myAggregator->batch;
        struct Batch<K, V> *myBatch = nullptr;
        int relativepushIndex = -1;

        while(1)
        {
            struct Batch<K, V> *prevBatch = mayBeMyBatch->bprev;

            if (pushIndex >= prevBatch->finalPushCount) // NOTEME: dummynode hass finalcount = -1
            {
                // I am in apt batch
                relativepushIndex  = (prevBatch->finalPushCount == -1)? (pushIndex) : (pushIndex - prevBatch->finalPushCount);


                myBatch = mayBeMyBatch;
                break; //I got myBatch and probably I will get into it or the next.
            }
            //iterate backwards untill you reach nill or your apt maybebatch.
            mayBeMyBatch = prevBatch;            
        }

        // assert (relative push index not -1)
        // pushIndex = relativepushIndex;
        // COUTATOMICTID("relativepushIndex: " << relativepushIndex  << "Absolute index " << pushIndex<< std::endl);

        while (true) {
    
            if (relativepushIndex == 0 && !myBatch->hasLeader.test_and_set())  // Should be test and set.
            {
                FreezeBatch(myAggregator, myBatch);
            } 
            else 
            {
                while (myBatch == myAggregator->batch)  // Spin until freezing has finished.
                {

                }
            }

            if (pushIndex >= myBatch->finalPushCount.load())  // I wasn't included.
            {
                // COUTATOMICTID("not included "<< "myBatch->finalPushCount= " << myBatch->finalPushCount<< " pushIndex= "<< pushIndex<<std::endl)
                relativepushIndex = pushIndex-myBatch->finalPushCount;
                myBatch = myBatch->bnext;
               
                continue; // retry in next batch which has to be your apt batch.
            }

            myBatch->eliminationArray[relativepushIndex].store(myNode);

            if (relativepushIndex < (myBatch->finalPopCount - myBatch->startingPopCntr))
            {
                // COUTATOMICTID("dummy eliminated " << value << std::endl);
                return true;
            }

            //first non eliminated thread become the leader
            if (relativepushIndex == (myBatch->finalPopCount - myBatch->startingPopCntr))
            {
                CreatePushSubstack(myBatch, relativepushIndex);
                PushToMain(myBatch->subStackTop, myBatch->subStackBot);
                myBatch->isBatchApplied.store(true);
            } 
            else 
            {
                while (myBatch->isBatchApplied.load() == false)  // Wait for leader to apply to main.
                {                     
                    // COUTATOMICTID("isBatchApplied "<<std::endl);
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

        Aggregator<K, V> *myAggregator = &CHOOSE_AGGREGATOR(tid);
        
        int popIndex = myAggregator->popCounter.fetch_add(1, tid);
        struct Batch<K, V> *mayBeMyBatch = myAggregator->batch;
        struct Batch<K, V> *myBatch = nullptr;
        int relativepopIndex = -1;

        while(1)
        {
            struct Batch<K, V> *prevBatch = mayBeMyBatch->bprev;

            if (popIndex >= prevBatch->finalPopCount) // NOTEME: dummynode hass finalcount = -1
            {
                // I am in apt batch
                relativepopIndex  = (prevBatch->finalPopCount == -1)? (popIndex) : (popIndex - prevBatch->finalPopCount);

                myBatch = mayBeMyBatch;
                break; //I got myBatch and probably I will get into it or the next.
            }
            //iterate backwards untill you reach nill or your apt maybebatch.
            mayBeMyBatch = prevBatch;            
        }


        while (true) {
            if (relativepopIndex == 0 && !myBatch->hasLeader.test_and_set()) {
                FreezeBatch(myAggregator, myBatch);
            } 
            else 
            {
                while (myBatch == myAggregator->batch) {}
            }

            if (popIndex >= myBatch->finalPopCount.load())  // Not included.
            {  
                relativepopIndex = popIndex-myBatch->finalPopCount;
                myBatch = myBatch->bnext;  
                continue;
            }

            if (relativepopIndex < (myBatch->finalPushCount - myBatch->startingPushCntr))  // Eliminated.
            {
                while (!myBatch->eliminationArray[relativepopIndex].load())  // Wait for Push
                                                 // to write value.
                {
                }

                nodeptr my_ptr = myBatch->eliminationArray[relativepopIndex].load();
                V returnValue = my_ptr->val;
                return returnValue;
            }

            if (relativepopIndex == (myBatch->finalPushCount - myBatch->startingPushCntr)) 
            {
                int remainingPops = (myBatch->finalPopCount - myBatch->startingPopCntr) - (myBatch->finalPushCount - myBatch->startingPushCntr);


                myBatch->subStackTop.store(PopFromMain(remainingPops));
                myBatch->isBatchApplied.store(true);
            } else {
                while (myBatch->isBatchApplied.load() == false) {}
            }
            return GetRetValue(
                relativepopIndex - (myBatch->finalPushCount - myBatch->startingPushCntr), myBatch->subStackTop.load()
            );
        }
        return true;

    }

    void initThread(const int tid) {
        // if (init[tid]) return;
        // else init[tid] = !init[tid];
        // recmgr->initThread(tid);  
#ifdef USE_POOLS
        if (!init) {
            synchInitPool(&pool_batch, sizeof(Batch<K, V>));
            synchInitPool(&pool_node, sizeof(node_t<K, V>));
            
            init = true;
        }
#endif
        if (0 == tid) COUTATOMICTID("comxagg batch size: " << sizeof(Batch<K, V>) << std::endl);
    }

    void deinitThread(const int tid) {
        // if (!init[tid]) return;
        // else init[tid] = !init[tid];
        // // recmgr->deinitThread(tid);
    }


};

#endif