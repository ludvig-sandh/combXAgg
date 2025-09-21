// Copyright (c) 2012-2013, the Scal Project Authors.  All rights reserved.
// Please see the AUTHORS file for details.  Use of this source code is governed
// by a BSD license that can be found in the LICENSE file.

// Implementing the non-blocking lock-free stack from:
//
// D. Hendler, N. Shavit, and L. Yerushalmi. A scalable lock-free stack
// algorithm. In Proc. Symposium on Parallelism in Algorithms and Architectures
// (SPAA), pages 206–215. ACM, 2004.

#ifndef SCAL_DATASTRUCTURES_ELIMINATION_BACKOFF_STACK_H_
#define SCAL_DATASTRUCTURES_ELIMINATION_BACKOFF_STACK_H_

#include <assert.h>
#include <inttypes.h>

#include <atomic>

// #include "datastructures/stack.h"
#include "allocation.h"
#include "atomic_value_new.h"
#include "platform.h"
#include "random.h"
#include "scal-time.h"
#include "threadlocals.h"

#define EMPTY 0

namespace scal {

namespace detail {

enum Opcode { Push = 1, Pop = 2 };

template <typename T>
struct Operation {
    Opcode opcode;
    T data;
};

template <typename T>
struct Node : ThreadLocalMemory<kCachePrefetch * 4> {
    explicit Node(T item) : next(NULL), data(item) {}

    Node<T> *next;
    T data;
};

}  // namespace detail

template <typename T>
class EliminationBackoffStack /* : public Stack<T>  */ {
   public:
    EliminationBackoffStack(uint64_t num_threads, uint64_t size_collision,
                            uint64_t delay);
    ~EliminationBackoffStack() {
      COUTATOMIC(" NUM THREADS " << num_threads_ << std::endl);
        for (size_t i = 0; i < num_threads_; i++) {
            free(operations_[i]);
            free(location_[i]);
        }
        for (size_t i = 0; i < size_collision_; i++) {
            free(collision_[i]);
        }
        delete top_;
    }
    bool push(T item, const int &tid);
    bool pop(T *item, const int &tid);

    char *ds_get_stats(void) {
        char buffer[255] = {0};
        uint32_t n = snprintf(buffer, sizeof(buffer),
                              " ,\"collision\": %ld ,\"delay\": %ld",
                              size_collision_, delay_);
        if (n != strlen(buffer)) {
            fprintf(stderr, "%s: error creating stats string\n", __func__);
            abort();
        }
        char *newbuf =
            static_cast<char *>(calloc(strlen(buffer) + 1, sizeof(*newbuf)));
        return strncpy(newbuf, buffer, strlen(buffer));
    }

   private:
    typedef detail::Node<T> Node;
    typedef detail::Opcode Opcode;
    typedef detail::Operation<T> Operation;

    typedef TaggedValue<Node *> NodePtr;
    typedef AtomicTaggedValue<Node *, 64, 64> AtomicNodePtr;

    AtomicNodePtr *top_;
    Operation **operations_;

    std::atomic<uint64_t> **location_;
    std::atomic<uint64_t> **collision_;
    const uint64_t num_threads_;
    const uint64_t size_collision_;
    const uint64_t delay_;

    bool try_collision(uint64_t thread_id, uint64_t other, T *item);
    bool backoff(Opcode opcode, T *item, const int &tid);
};

template <typename T>
EliminationBackoffStack<T>::EliminationBackoffStack(uint64_t num_threads,
                                                    uint64_t size_collision,
                                                    uint64_t delay)
    : num_threads_(num_threads),
      size_collision_(size_collision),
      delay_(delay) {
    VERBOSE COUTATOMIC("ctorbegin EliminationBackoffStack" << std::endl);

    top_ = new AtomicNodePtr();

    operations_ =
        static_cast<Operation **>(ThreadLocalAllocator::Get().CallocAligned(
            num_threads, sizeof(Operation *), kCachePrefetch * 4));

    location_ = static_cast<std::atomic<uint64_t> **>(
        ThreadLocalAllocator::Get().CallocAligned(
            num_threads, sizeof(std::atomic<uint64_t> *), kCachePrefetch * 4));

    collision_ = static_cast<std::atomic<uint64_t> **>(
        ThreadLocalAllocator::Get().CallocAligned(
            size_collision_, sizeof(TaggedValue<uint64_t> *),
            kCachePrefetch * 4));

    void *mem;
    for (uint64_t i = 0; i < num_threads; i++) {
        mem = MallocAligned(sizeof(Operation), kCachePrefetch * 4);
        operations_[i] = new (mem) Operation();
    }
    for (uint64_t i = 0; i < num_threads; i++) {
        mem = MallocAligned(sizeof(std::atomic<uint64_t>), kCachePrefetch * 4);
        location_[i] = new (mem) std::atomic<uint64_t>();
    }

    for (uint64_t i = 0; i < size_collision_; i++) {
        mem = MallocAligned(sizeof(std::atomic<uint64_t>), kCachePrefetch * 4);
        collision_[i] = new (mem) std::atomic<uint64_t>();
    }
}

inline uint64_t get_pos(uint64_t size) { return hwrand() % size; }

template <typename T>
bool EliminationBackoffStack<T>::try_collision(uint64_t thread_id,
                                               uint64_t other, T *item) {
    TaggedValue<T> old_value(other, 0);
    if (operations_[thread_id]->opcode == Opcode::Push) {
        TaggedValue<T> new_value(thread_id, 0);
        if (location_[other]->compare_exchange_weak(other, thread_id)) {
            return true;
        } else {
            return false;
        }
    } else {
        TaggedValue<T> new_value(EMPTY, 0);
        if (location_[other]->compare_exchange_weak(other, EMPTY)) {
            *item = operations_[other]->data;
            return true;
        } else {
            return false;
        }
    }
}

template <typename T>
bool EliminationBackoffStack<T>::backoff(Opcode opcode, T *item,
                                         const int &tid) {
    // uint64_t thread_id = ThreadContext::get().thread_id();
    uint64_t thread_id = tid;

    operations_[thread_id]->opcode = opcode;
    operations_[thread_id]->data = *item;
    location_[thread_id]->store(thread_id);
    uint64_t position = get_pos(size_collision_);
    uint64_t him = collision_[position]->load();
    while (!collision_[position]->compare_exchange_weak(him, thread_id)) {
    }
    if (him != EMPTY) {
        uint64_t other = location_[him]->load();
        if (other == him && operations_[other]->opcode != opcode) {
            uint64_t expected = thread_id;
            if (location_[thread_id]->compare_exchange_weak(expected, EMPTY)) {
                if (try_collision(thread_id, other, item)) {
                    return true;
                } else {
                    return false;
                }
            } else {
                if (opcode == Opcode::Pop) {
                    *item = operations_[location_[thread_id]->load()]->data;
                    location_[thread_id]->store(0);
                }
                return true;
            }
        }
    }

    // Wait some time for collisions.
    uint64_t wait = get_hwtime() + delay_;
    while (get_hwtime() < wait) {
        __asm__("PAUSE");
    }

    uint64_t expected = thread_id;
    if (!location_[thread_id]->compare_exchange_strong(expected, EMPTY)) {
        if (opcode == Opcode::Pop) {
            *item = operations_[location_[thread_id]->load()]->data;
            location_[thread_id]->store(EMPTY);
        }
        return true;
    }

    return false;
}
template <typename T>
bool EliminationBackoffStack<T>::push(T item, const int &tid) {
    if (backoff(Opcode::Push, &item, tid)) {
        return true;
    }
    Node *n = new Node(item);
    NodePtr top_old;
    NodePtr top_new;
    while (true) {
        top_old = top_->load();
        n->next = top_old.value();
        top_new = NodePtr(n, top_old.tag() + 1);

        if (!top_->swap(top_old, top_new)) {
            if (backoff(Opcode::Push, &item, tid)) {
                return true;
            }
        } else {
            break;
        }
    }
    return true;
}

template <typename T>
bool EliminationBackoffStack<T>::pop(T *item, const int &tid) {
    if (backoff(Opcode::Pop, item, tid)) {
        return true;
    }
    NodePtr top_old;
    NodePtr top_new;
    while (true) {
        top_old = top_->load();
        if (top_old.value() == NULL) {
            return false;
        }
        top_new = NodePtr(top_old.value()->next, top_old.tag() + 1);

        if (!top_->swap(top_old, top_new)) {
            if (backoff(Opcode::Pop, item, tid)) {
                return true;
            }
        } else {
            break;
        }
    }
    *item = top_old.value()->data;
    return true;
}

}  // namespace scal

#endif  // SCAL_DATASTRUCTURES_ELIMINATION_BACKOFF_STACK_H_
