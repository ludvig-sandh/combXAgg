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
#include "elimination_backoff_stack.h"

// template <typename K, typename V>
// class node_t {
//    public:
//     K key;
//     V val;
//     std::atomic<node_t<K, V> *> next;
//     node_t(const K &k, const V &v) : key(k), val(v), next(nullptr) {}
// };
// #define nodeptr node_t<K, V> *

// template <typename K, typename V>
// class top_node {
//    public:
//     nodeptr topptr;
//     std::mutex top_lock;

//     top_node() { topptr = nullptr; }
// };
// #define top_ top_node<K, V>

//useless fwd declaration. I don't intend to use this node. Using EBS internal node type instead. This means setbench reclaimer won't be able to reclaim this stack.
template<typename K, typename V>
struct node_t
{
    K key;
    V val;
};

template <typename K, typename V, class RecManager>
class Stack {
   private:
    PAD;
    // top_ top;
    scal::EliminationBackoffStack<uint64_t> *ebs;
    PAD;

   public:
    Stack(const int num_threads, const int _min_key, const int _max_key,
          const V _NO_VALUE, unsigned int id) {

        scal::ThreadLocalAllocator::Get().Init(1*1024*1024/* 10mb */, true);


            // uint64 FLAGS_collision = 0; //, "size of the collision array");
        uint64_t FLAGS_delay = 15000; //, "time waiting in the collision array");

        uint64_t size_collision = (num_threads + 1)/10;
        // if (FLAGS_collision != 0) {
        //     size_collision = FLAGS_collision;
        // }
        if (size_collision == 0) { 
            size_collision = 1;
        }

        VERBOSE COUTATOMIC("STACK" << std::endl);   
        ebs = new scal::EliminationBackoffStack<uint64_t>(num_threads + 1, size_collision, FLAGS_delay);
    }
    ~Stack() {}

    void initThread(const int tid) {
        VERBOSE COUTATOMIC("begin initThread" << std::endl);
        // const size_t tlsize = scal::HumanSizeToPages("m\n", 10);
        scal::ThreadLocalAllocator::Get().Init(1*1024*1024/* 10mb */, true);
        
        VERBOSE COUTATOMIC("end initThread" << std::endl);
    }

    V peek(const int &tid) {
        V retVal;
        // top.top_lock.lock();
        // if (top.topptr) {
        //     retVal = top.topptr->val;
        // } else {
        //     retVal = V();
        // }
        // top.top_lock.unlock();

        return retVal;
    }

    bool push(const int &tid, const K value) {
        bool success = false;
        // nodeptr my_node = new node_t<K, V>(0, value);
        // top.top_lock.lock();
        // my_node->next = top.topptr;
        // top.topptr = my_node;
        // // COUTATOMICTID("dummy pushing " << value << std::endl);
        // success = true;
        // top.top_lock.unlock();
        success = ebs->push(value, tid);

        return success;
    }

    bool pop(const int &tid) {
        bool success = false;
        long unsigned int value;
        // top.top_lock.lock();
        // nodeptr temp = top.topptr;
        // if (!temp) {
        //     top.top_lock.unlock();
        //     return success;
        // }
        // top.topptr = temp->next;
        // // COUTATOMICTID("DUMMY popping " << std::endl);
        // top.top_lock.unlock();
        // success = true;
        success = ebs->pop(&value, tid);
        
        return success;
    }
};

#endif