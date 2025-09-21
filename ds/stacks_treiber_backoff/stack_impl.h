


/**
 * @file stack_impl.h
 * @author
 * @brief Treiber stack with exponential backoff
 * @version 0.2
 * @date 2025-05-09
 */

#ifndef STACK_IMPL_H
#define STACK_IMPL_H

#include <atomic>
#include <thread>
#include <random>
#include <chrono>
#include "record_manager.h"

template <typename K, typename V>
class node_t {
   public:
    K key;
    V val;
    std::atomic<node_t<K, V> *> next;
    node_t(K k, V v) : key(k), val(v), next(nullptr) {}
};
#define nodeptr node_t<K, V> *

// --------------------- Backoff Class ---------------------
class Backoff {
   private:
    int minDelay;
    int maxDelay;
    int limit;
    std::default_random_engine generator;
    std::uniform_int_distribution<int> dist;

   public:
    Backoff(int min, int max)
        : minDelay(min),
          maxDelay(max),
          limit(min),
          dist(0, 1) {
        unsigned seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        generator.seed(seed);
    }

    void backoff() {
        int delay = dist(generator) % limit;
        limit = std::min(maxDelay, 2 * limit);  // exponential growth
        std::this_thread::sleep_for(std::chrono::nanoseconds(delay));
    }
};

// --------------------- Treiber Stack ---------------------
template <typename K, typename V, class RecMgr>
class Stack {
   private:
    PAD;
    std::atomic<nodeptr> _top;
    PAD;
    RecMgr *const recmgr;
    PAD;
    int init[MAX_THREADS_POW2] = {
        0,
    };
    static constexpr int MIN_DELAY = 1;       // nanoseconds
    static constexpr int MAX_DELAY = 1000;    // nanoseconds

   public:
    Stack(const int num_threads, const int _min_key, const int _max_key,
          const V _NO_VALUE, unsigned int id)
        : _top(nullptr), recmgr(new RecMgr(num_threads)) {}
    ~Stack() {
        while (_top.load() != nullptr) {
            nodeptr old_top = _top.load();
            _top.store(old_top->next);
            delete old_top;
        }
        recmgr->printStatus();
        delete recmgr;
        
    }

    V peek(const int &tid) {
        nodeptr t = _top.load(std::memory_order_acquire);
        return t ? t->val : V();
    }

    bool push(const int &tid, const V &value) {
        recmgr->startOp(tid);
        nodeptr new_top = new node_t<K, V>(0, value);
        Backoff backoff(MIN_DELAY, MAX_DELAY);
        nodeptr old_top;

        while (true) {
            old_top = _top.load(std::memory_order_acquire);
            new_top->next.store(old_top, std::memory_order_relaxed);

            if (_top.compare_exchange_strong(old_top, new_top,
                                             std::memory_order_acq_rel)) {
                recmgr->endOp(tid);
                return true;
            } else {
                backoff.backoff();
            }
        }
    }

    V pop(const int &tid) {
        recmgr->startOp(tid);
        Backoff backoff(MIN_DELAY, MAX_DELAY);
        nodeptr old_top;
        nodeptr new_top;

        while (true) {
            old_top = _top.load(std::memory_order_acquire);
            if (old_top == nullptr) {
                recmgr->endOp(tid);
                return V();  // stack empty
            }
            new_top = old_top->next.load(std::memory_order_relaxed);

            if (_top.compare_exchange_strong(old_top, new_top,
                                             std::memory_order_acq_rel)) {
                V val = old_top->val;
                recmgr->retire(tid, old_top);
                // delete old_top;  // free memory (or use RecManager)
                recmgr->endOp(tid);
                return val;
            } else {
                backoff.backoff();
            }
        }
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

#endif
