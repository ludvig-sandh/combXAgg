
/// @file ccsynch.h
/// @author Nikolaos D. Kallimanis (nkallima@gmail.com)
/// @brief This file exposes the API of the CC-Synch combining object.
/// An example of use of this API is provided in benchmarks/ccsynchbench.c file.
///
/// For a more detailed description see the original publication:
/// Panagiota Fatourou, and Nikolaos D. Kallimanis."Revisiting the combining
/// synchronization technique". ACM SIGPLAN Notices. Vol. 47. No. 8. ACM, PPoPP
/// 2012.
/// @copyright Copyright (c) 2021
#ifndef _CCSYNCH_H_
#define _CCSYNCH_H_

#include <config.h>
#include <primitives.h>

/// @brief HalfCCSynchNode should not be directly used by the user.
/// It is internally used for proper alignment of the CCSynchNode struct.
typedef struct HalfCCSynchNode {
    struct HalfCCSynchNode* next;
    ArgVal arg_ret;
    int32_t pid;
    int32_t locked;
    int32_t completed;
} HalfCCSynchNode;

/// @brief CCSynchNode stores the data of an announced request.
typedef struct CCSynchNode {
    /// @brief Pointer to the next request that has been announced.
    struct CCSynchNode* next;
    /// @brief This variable stores the argument of the request and the return
    /// value after the request is applied.
    ArgVal arg_ret;
    /// @brief The pid of the thread that announced this request.
    int32_t pid;
    /// @brief Whenever it is equal to false, the thread is the combiner;
    /// otherwise the thread waits until a combiner apply its request.
    int32_t locked;
    /// @brief If true, the request is applied and the thread returns its return
    /// value.
    int32_t completed;
    /// @brief Padding space.
    char align[PAD_CACHE(sizeof(HalfCCSynchNode))];
} CCSynchNode;

/// @brief CCSynchThreadState stores each thread's local state for a single
/// instance of CC-Synch. For each instance of CC-Synch, a discrete instance of
/// CCSynchThreadState should be used.
typedef struct CCSynchThreadState {
    /// @brief pointer to an empty request that would be used for announcing
    /// future requests.
    CCSynchNode* next;
    /// @brief A toggle-bit used by the CC-Synch object.
    int toggle;
} CCSynchThreadState;

/// @brief CCSynchStruct stores the state of an instance of the a CC-Synch
/// combining object. CCSynchStruct should be initialized using the
/// CCSynchStructInit function.
typedef struct CCSynchStruct {
    /// @brief Tail points to the most recently announced request. Initially, it
    /// points to NULL.
    volatile CCSynchNode* Tail CACHE_ALIGN;
    /// @brief Pointer to the pool of nodes used by threads in order to announce
    /// their requests.
    CCSynchNode* nodes CACHE_ALIGN;
    /// @brief The number of threads that will use the CC-Synch combining
    /// object.
    uint32_t nthreads;
#ifdef DEBUG
    volatile uint64_t counter CACHE_ALIGN;
    volatile int rounds;
#endif
} CCSynchStruct;

/// @brief This function initializes an instance of the CC-Synch combining
/// object.
///
/// This function should be called once (by a single thread) before any other
/// thread tries to apply any request by using the CCSynchApplyOp function.
///
/// @param l A pointer to an instance of the CC-Synch combining object.
/// @param nthreads The number of threads that will use the CC-Synch combining
/// object.
void CCSynchStructInit(CCSynchStruct* l, uint32_t nthreads);

/// @brief This function should be called once before the thread applies any
/// operation to the CC-Synch combining object.
///
/// @param l A pointer to an instance of the CC-Synch combining object.
/// @param st_thread A pointer to thread's local state of CC-Synch.
/// @param pid The pid of the calling thread.
void CCSynchThreadStateInit(CCSynchStruct* l, CCSynchThreadState* st_thread,
                            int pid);

/// @brief This function is called whenever a thread wants to apply an operation
/// to the simulated concurrent object.
///
/// @param l A pointer to an instance of the CC-Synch combining object.
/// @param st_thread A pointer to thread's local state for a specific instance
/// of CC-Synch.
/// @param sfunc A serial function that the CC-Synch instance should execute,
/// while applying requests announced by active threads.
/// @param state A pointer to the state of the simulated object.
/// @param arg The argument of the request that the thread wants to apply.
/// @param pid The pid of the calling thread.
/// @return RetVal The return value of the applied request.
RetVal CCSynchApplyOp(CCSynchStruct* l, CCSynchThreadState* st_thread,
                      RetVal (*sfunc)(void*, ArgVal, int), void* state,
                      ArgVal arg, int pid);

#include <stdbool.h>
#include <threadtools.h>

static const int CCSYNCH_HELP_FACTOR = 10;

RetVal CCSynchApplyOp(CCSynchStruct* l, CCSynchThreadState* st_thread,
                      RetVal (*sfunc)(void*, ArgVal, int), void* state,
                      ArgVal arg, int pid) {
    volatile CCSynchNode* p;
    volatile CCSynchNode* cur;
    CCSynchNode *next_node, *tmp_next;
    int help_bound = CCSYNCH_HELP_FACTOR * l->nthreads;
    int counter = 0;

    next_node = st_thread->next;
    next_node->next = NULL;
    next_node->locked = true;
    synchNonTSOFence();
    next_node->completed = false;

    cur = (CCSynchNode*)synchSWAP(&l->Tail, next_node);
    cur->arg_ret = arg;
    cur->pid = pid;
    synchNonTSOFence();
    cur->next = (CCSynchNode*)next_node;
    st_thread->next = (CCSynchNode*)cur;
    synchNonTSOFence();

    while (cur->locked) {  // spinning
        synchResched();
    }
    if (cur->completed)  // I have been helped
        return cur->arg_ret;
#ifdef DEBUG
    {
        int tmp = l->rounds;
        tmp++;
        l->rounds = tmp;
    }
#endif
    p = cur;  // I am not been helped
    while (p->next != NULL && counter < help_bound) {
        synchStorePrefetch(p->next);

        counter++;
#ifdef DEBUG
        {
            int tmp = l->counter;
            tmp++;
            l->counter = tmp;
        }
#endif
        tmp_next = p->next;
        p->arg_ret = sfunc(state, p->arg_ret, p->pid);
        synchNonTSOFence();
        p->completed = true;
        synchNonTSOFence();
        p->locked = false;
        p = tmp_next;
    }
    synchNonTSOFence();
    p->locked = false;  // Unlock the next one
    synchStoreFence();

    return cur->arg_ret;
}

void CCSynchStructInit(CCSynchStruct* l, uint32_t nthreads) {
    l->nthreads = nthreads;

    if (synchGetMachineModel() == INTEL_X86_MACHINE) {
        l->nodes = NULL;
        l->Tail = reinterpret_cast<volatile CCSynchNode*>(
            synchGetAlignedMemory(CACHE_LINE_SIZE, sizeof(CCSynchNode)));
    } else {
        l->nodes = reinterpret_cast<CCSynchNode*>(synchGetAlignedMemory(
            CACHE_LINE_SIZE, (nthreads + 1) * sizeof(CCSynchNode)));
        l->Tail = &l->nodes[nthreads];
    }

#ifdef DEBUG
    l->rounds = 0;
    l->counter = 0;
#endif

    l->Tail->next = NULL;
    l->Tail->locked = false;
    l->Tail->completed = false;

    synchStoreFence();
}

void CCSynchThreadStateInit(CCSynchStruct* l, CCSynchThreadState* st_thread,
                            int pid) {
    if (synchGetMachineModel() == INTEL_X86_MACHINE) {
        st_thread->next = reinterpret_cast<CCSynchNode*>(
            synchGetAlignedMemory(CACHE_LINE_SIZE, sizeof(CCSynchStruct)));
    } else {
        st_thread->next = &l->nodes[pid];
    }
}
#endif
