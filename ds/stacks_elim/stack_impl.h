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

#include "elimination_backoff_stack.h"
#include "record_manager.h"

template <typename K, typename V>
class node_t {
   public:
    K key;
    V val;
    std::atomic<node_t<K, V> *> next;
};
#define nodeptr node_t<K, V> *

template <typename K, typename V, class RecManager>
class Stack_WRAP {
   private:
    scal::EliminationBackoffStack<K> stack;

   public:
    Stack_WRAP(const int num_threads, const int _min_key, const int _max_key,
               const V _NO_VALUE, unsigned int id)
        : stack(num_threads + 1, (num_threads + 1) / 10, 15000) {}

    ~Stack_WRAP() {}

    V peek(const int &tid) { return NULL; }

    bool push(const int &tid, const V &value) { return stack.push(value); }

    bool pop(const int &tid) {
        K item;
        return stack.pop(&item);
    }
};

#endif