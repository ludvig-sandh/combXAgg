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
    }
};
#define nodeptr node_t<K, V> *

template <typename K, typename V, class RecManager>
class Stack {
   private:
    PAD;
    std::atomic<nodeptr> _top;
    PAD;

   public:
    Stack(const int num_threads, const int _min_key, const int _max_key,
          const V _NO_VALUE, unsigned int id)
        : _top(NULL) {}
    ~Stack() {}

    V peek(const int &tid) { return _top ? _top.load() : V(); }

    bool push(const int &tid, const V &value) {
        nodeptr new_top = new node_t<K, V>(0, value);
        nodeptr old_top;
        do {
            old_top = _top.load();
            new_top->next = old_top;
        } while (!_top.compare_exchange_strong(old_top, new_top,
                                               std::memory_order_acq_rel));
        COUTATOMICTID("dummy pushing " << value << std::endl);
        return true;
    }

    bool pop(const int &tid) {
        bool success = false;
        nodeptr old_top;
        nodeptr new_top;
        do {
            old_top = _top.load();
            if (old_top == nullptr) {
                return V();
            }
            new_top = old_top->next;
        } while (!_top.compare_exchange_strong(old_top, new_top,
                                               std::memory_order_acq_rel));

        COUTATOMICTID("DUMMY popping " << std::endl);
        return success;
    }
};

#endif