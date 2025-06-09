#ifndef STACK_IMPL_H
#define STACK_IMPL_H

#include <gflags/gflags.h>

#include "kstack.h"
#include "record_manager.h"
DEFINE_uint64(k, 80, "k-segment size");
template <typename K, typename V>
class node_t {
   public:
    K key;
    V val;
    std::atomic<node_t<K, V> *> next;
};

template <typename K, typename V, class RecManager>
class Stack_WRAP {
   private:
    scal::KStack<V> *kstack;  // Pointer to KStack instance

   public:
    Stack_WRAP(const int num_threads, const int _min_key, const int _max_key,
               const V _NO_VALUE, unsigned int id) {
        kstack = new scal::KStack<V>(FLAGS_k, num_threads + 1);
    }

    ~Stack_WRAP() {
        delete kstack;  // Clean up allocated KStack instance
    }

    V peek(const int &tid) {
        V value;  // Implement peek logic if needed
        return value;
    }

    bool push(const int &tid, const V &value) {
        return kstack->push(value);  // Delegate push operation to KStack
    }

    bool pop(const int &tid) {
        V value;
        return kstack->pop(&value);  // Delegate pop operation to KStack
    }
};

#endif
