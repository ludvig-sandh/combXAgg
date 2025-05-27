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

#include <mutex>

#include "record_manager.h"

template <typename K, typename V>
class node_t {
   public:
    K key;
    V val;
    std::atomic<node_t<K, V> *> next;
    node_t(const K &k, const V &v) : key(k), val(v), next(nullptr) {}
};
#define nodeptr node_t<K, V> *

template <typename K, typename V>
class top_node {
   public:
    nodeptr topptr;
    std::mutex top_lock;

    top_node() { topptr = nullptr; }
};
#define top_ top_node<K, V>
template <typename K, typename V, class RecManager>
class Stack {
   private:
    PAD;
    top_ top;
    PAD;

   public:
    Stack(const int num_threads, const int _min_key, const int _max_key,
          const V _NO_VALUE, unsigned int id) {}
    ~Stack() {}

    V peek(const int &tid) {
        V retVal;
        top.top_lock.lock();
        if (top.topptr) {
            retVal = top.topptr->val;
        } else {
            retVal = V();
        }
        top.top_lock.unlock();

        return retVal;
    }

    bool push(const int &tid, const V &value) {
        bool success = false;
        nodeptr my_node = new node_t<K, V>(0, value);
        top.top_lock.lock();
        my_node->next = top.topptr;
        top.topptr = my_node;
        // COUTATOMICTID("dummy pushing " << value << std::endl);
        success = true;
        top.top_lock.unlock();
        return success;
    }

    bool pop(const int &tid) {
        bool success = false;
        top.top_lock.lock();
        nodeptr temp = top.topptr;
        if (!temp) {
            top.top_lock.unlock();
            return success;
        }
        top.topptr = temp->next;
        // COUTATOMICTID("DUMMY popping " << std::endl);
        top.top_lock.unlock();
        success = true;
        return success;
    }
};

#endif