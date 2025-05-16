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

template<typename K, typename V>
class node_t {
public:
    K key;
    V val;
    std::atomic<node_t<K,V>*> next;
};
#define nodeptr node_t<K,V> *

template <typename K, typename V, class RecManager>
class Stack
{
private:
PAD;
std::atomic<nodeptr> _top;
PAD;

public:
    Stack(const int num_threads, const int _min_key,
    const int _max_key, const V _NO_VALUE, unsigned int id): _top(NULL)
    {
    }
    ~Stack()
    {

    }

    V peek(const int &tid) 
    {  
        return NULL;
    }

    bool push(const int &tid, const V &value) 
    {
        bool success = false;
        COUTATOMICTID("dummy pushing "<<value<<std::endl);
        return success;
    }

    bool pop(const int &tid)
    {
        bool success = false;
        COUTATOMICTID("DUMMY popping "<<std::endl);
        return success;
    }
};

#endif