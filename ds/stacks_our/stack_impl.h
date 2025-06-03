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
    std::atomic<nodeptr> eliminationArray[MAX_AGGREGATOR_THREADS];
    std::atomic<int> pushCounter;
    std::atomic<int> popCounter;
    std::atomic<int> finalPushCount;
    std::atomic<int> finalPopCount;
    std::atomic_flag hasLeader;
    std::atomic<bool> isBatchApplied;
    std::atomic<nodeptr> subStackTop;
    std::atomic<nodeptr> subStackBot;
    struct Batch *next;
};

template <typename K, typename V>
struct Aggregator {
    std::atomic<struct Batch<K, V> *> batch;
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
                     std::atomic<struct Batch<K, V> *> batch) {
        std::atomic<struct Batch<K, V> *> newBatch;
        newBatch.store(CreateNewBatch());
        newBatch.load()->next = batch.load();
        // Snapshots the counters
        batch.load()->finalPushCount.store(batch.load()->pushCounter.load());
        batch.load()->finalPopCount.store(batch.load()->popCounter.load());
        // Unlocks the waiting threads
        aggregator->batch.store(newBatch);
    }
    void PushToMain(
        nodeptr subStackTop,
        nodeptr subStackBot)  // Aggregation of multiple push operations
                              // to main(same as aggregation of F&A).
    {
        while (true) {
            struct node_t<K, V> *top = main_top;
            subStackBot->next = top;
            if (main_top.compare_exchange_strong(top, subStackTop)) return;
        }
    }
    void CreatePushSubstack(struct Batch<K, V> *batch, int leaderIndex) {
        batch->subStackBot.store(batch->eliminationArray[leaderIndex].load());
        struct node_t<K, V> *tempTop = batch->subStackBot;
        int i = 1;
        while (leaderIndex + i < batch->finalPushCount) {
            while (
                !batch->eliminationArray[leaderIndex +
                                         i])  // Wait for Push to write value.
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

            // COUTATOMICTID("dummy pushing " << value << std::endl);
            if (pushIndex == 0 &&
                !myBatch->hasLeader.test_and_set())  // Should be test and set.
            {
                FreezeBatch(myAggregator, myBatch);
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

            myBatch->eliminationArray[pushIndex].store(myNode);
            if (pushIndex < myBatch->finalPopCount.load())  // Eliminated.
            {
                // COUTATOMICTID("dummy eliminated " << value << std::endl);
                return true;
            }
            if (pushIndex == myBatch->finalPopCount.load()) {
                CreatePushSubstack(myBatch, pushIndex);
                PushToMain(myBatch->subStackTop, myBatch->subStackBot);
                myBatch->isBatchApplied.store(true);
            } else {
                while (myBatch->isBatchApplied.load() ==
                       false)  // Wait for leader to apply to main.
                {
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
        bool success = false;
        while (true) {
            struct Batch<K, V> *myBatch = myAggregator->batch;
            int popIndex = myBatch->popCounter.fetch_add(1);
            // COUTATOMICTID("DUMMY popping " << std::endl);
            if (popIndex == 0 && !myBatch->hasLeader.test_and_set()) {
                FreezeBatch(myAggregator, myBatch);
            } else {
                while (myBatch == myAggregator->batch) {
                }
            }
            if (popIndex >= myBatch->finalPopCount.load())  // Not included.
                continue;
            if (popIndex < myBatch->finalPushCount.load())  // Eliminated.
            {
                // COUTATOMICTID("DUMMY elimin pop , popIndex = "
                            //   << popIndex << " , Push count = "
                            //   << myBatch->finalPushCount.load()
                            //   << " FinalPopCount = "
                            //   << myBatch->finalPopCount.load() << std::endl);
                while (!myBatch->eliminationArray[popIndex]
                            .load())  // Wait for Push
                                      // to write value.
                {
                }
                nodeptr my_ptr = myBatch->eliminationArray[popIndex].load();
                return my_ptr->val;
            }
            if (popIndex == myBatch->finalPushCount.load()) {
                int remainingPops = myBatch->finalPopCount.load() -
                                    myBatch->finalPushCount.load();
                myBatch->subStackTop = PopFromMain(remainingPops);
                myBatch->isBatchApplied.store(true);
            } else {
                while (myBatch->isBatchApplied.load() == false) {
                }
            }
            return GetRetValue(popIndex - myBatch->finalPushCount.load(),
                               myBatch->subStackTop.load());
        }
        return success;
    }
};

#endif