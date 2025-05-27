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

#define MAX_AGGREGATOR_THREADS 12
#define NUMBER_AGGREGATORS 64

#define CHOOSE_AGGREGATOR(tId) (aggregator[(tId) / MAX_AGGREGATOR_THREADS])

#include "record_manager.h"

template <typename K, typename V>
class node_t {
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
struct Batch {
    nodeptr eliminationArray[MAX_AGGREGATOR_THREADS];
    std::atomic<int> pushCounter;
    std::atomic<int> popCounter;
    int finalPushCount;
    int finalPopCount;
    std::atomic_flag hasLeader;
    volatile bool isBatchApplied;
    nodeptr subStackTop;
    nodeptr subStackBot;
    struct Batch *next;
};

template <typename K, typename V>
struct Aggregator {
    struct Batch<K, V> *batch;
};

template <typename K, typename V, class RecManager>
class Stack {
   private:
    PAD;
    std::atomic<nodeptr> main_top;
    PAD;
    Aggregator<K, V> aggregator[NUMBER_AGGREGATORS];

    struct Batch<K, V> *CreateNewBatch() {
        struct Batch<K, V> *newBatch = new Batch<K, V>;
        newBatch->popCounter = 0;
        newBatch->pushCounter = 0;
        newBatch->finalPopCount = 0;
        newBatch->finalPushCount = 0;
        newBatch->hasLeader.clear();
        newBatch->isBatchApplied = false;
        newBatch->subStackBot = NULL;
        newBatch->subStackTop = NULL;
        newBatch->next = NULL;
        for (size_t i = 0; i < MAX_AGGREGATOR_THREADS; i++) {
            newBatch->eliminationArray[i] = NULL;
        }
        return newBatch;
    }

    void FreezeBatch(struct Aggregator<K, V> *aggregator,
                     struct Batch<K, V> *batch) {
        struct Batch<K, V> *newBatch = CreateNewBatch();
        newBatch->next = batch;
        // Snapshots the counters
        batch->finalPushCount = batch->pushCounter;
        batch->finalPopCount = batch->popCounter;
        // Unlocks the waiting threads
        aggregator->batch = newBatch;
    }
    void PushToMain( nodeptr subStackTop, nodeptr subStackBot)  // Aggregation of multiple push operations
                              // to main(same as aggregation of F&A).
    {
        while (true) {
            struct node_t<K, V> *top = main_top;
            subStackBot->next = top;
            if (main_top.compare_exchange_strong(top, subStackTop)) return;
        }
    }
    void CreatePushSubstack(struct Batch<K, V> *batch, int leaderIndex) {
        batch->subStackBot = batch->eliminationArray[leaderIndex];
        struct node_t<K, V> *tempTop = batch->subStackBot;
        int i = 1;
        while (leaderIndex + i < batch->finalPushCount) {
            while (!batch->eliminationArray[leaderIndex + i])  // Wait for Push to write value.
            {
            }
            nodeptr tempNode = batch->eliminationArray[leaderIndex + i];
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

    V peek(const int &tid) { return main_top ? main_top.load() : V(); }

    bool push(const int &tid, const V &value) {
        Aggregator<K, V> *myAggregator = &CHOOSE_AGGREGATOR(tid);
        nodeptr myNode = new node_t<K, V>(0, value);
        while (true) {
            struct Batch<K, V> *myBatch = myAggregator->batch;
            int pushIndex = myBatch->pushCounter.fetch_add(
                1);  // Opt. Check software F&A speed up?
            // myBatch->eliminationArray[pushIndex] = myNode;
            if (pushIndex == 0 && !myBatch->hasLeader.test_and_set())  // Should be test and set.
            {
                FreezeBatch(myAggregator, myBatch);
            }
            else 
            {
                while (myBatch == myAggregator->batch)  // Spin until freezing has finished.
                {
                }
            }
            if (pushIndex >= myBatch->finalPushCount)  // I wasn't included.
                continue;  // Try to join a new batch

            myBatch->eliminationArray[pushIndex] = myNode;
            if (pushIndex < myBatch->finalPopCount)  // Eliminated.
            {
                // COUTATOMICTID("dummy eliminated " << value << std::endl);
                return true;
            }
            if (pushIndex == myBatch->finalPopCount) {
                CreatePushSubstack(myBatch, pushIndex);
                PushToMain(myBatch->subStackTop, myBatch->subStackBot);
                myBatch->isBatchApplied = true; //FIXME: shared var should be atomic
            } else {
                while (myBatch->isBatchApplied == false)  // Wait for leader to apply to main.
                {
                }
            }
            // COUTATOMICTID("dummy pushing " << value << std::endl);
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
        bool success = false;
        // COUTATOMICTID("DUMMY popping " << std::endl);
        while (true) {
            struct Batch<K, V> *myBatch = myAggregator->batch;
            int popIndex = myBatch->popCounter.fetch_add(1);
            if (popIndex == 0 && !myBatch->hasLeader.test_and_set()) {
                FreezeBatch(myAggregator, myBatch);
            } 
            else 
            {
                while (myBatch == myAggregator->batch) {
                }
            }
            if (popIndex >= myBatch->finalPopCount)  // Not included.
                continue;
            if (popIndex < myBatch->finalPushCount)  // Eliminated.
            {
                while (!myBatch->eliminationArray[popIndex])  // Wait for Push to
                                                             // write value.
                {
                }
                return myBatch->eliminationArray[popIndex]->val;
            }
            if (popIndex == myBatch->finalPushCount) 
            {
                int remainingPops = myBatch->finalPopCount - myBatch->finalPushCount;
                myBatch->subStackTop = PopFromMain(remainingPops);
                myBatch->isBatchApplied = true; //FIX: shared var need to be atomic
            } else 
            {
                while (myBatch->isBatchApplied == false) {
                }
            }
            return GetRetValue(popIndex - myBatch->finalPushCount,
                               myBatch->subStackTop);
        }
        return success;
    }
};

#endif