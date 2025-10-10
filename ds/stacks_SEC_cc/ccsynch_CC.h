/// @file ccsynch.h
/// @author Nikolaos D. Kallimanis (nkallima@gmail.com)
/// @brief This file exposes the API of the CC-Synch combining object.
/// An example of use of this API is provided in benchmarks/ccsynchbench.c file.
///
/// For a more detailed description see the original publication:
/// Panagiota Fatourou, and Nikolaos D. Kallimanis."Revisiting the combining synchronization technique".
/// ACM SIGPLAN Notices. Vol. 47. No. 8. ACM, PPoPP 2012.
/// @copyright Copyright (c) 2021
#ifndef _CCSYNCH_H_
#define _CCSYNCH_H_

#include <config_CC.h>
#include <primitives_CC.h>

/// @brief HalfCCSynchNode should not be directly used by the user.
/// It is internally used for proper alignment of the CCSynchNode struct.
typedef struct HalfCCSynchNode {
    struct HalfCCSynchNode *next;
    ArgVal arg_ret;
    int32_t pid;
    int32_t locked;
    int32_t completed;
} HalfCCSynchNode;

/// @brief CCSynchNode stores the data of an announced request.
typedef struct CCSynchNode {
    /// @brief Pointer to the next request that has been announced.
    struct CCSynchNode *next;
    /// @brief This variable stores the argument of the request and the return value after the request is applied.
    ArgVal arg_ret;
    /// @brief The pid of the thread that announced this request.
    int32_t pid;
    /// @brief Whenever it is equal to false, the thread is the combiner; otherwise the thread waits until a combiner apply its request.
    int32_t locked;
    /// @brief If true, the request is applied and the thread returns its return value.
    int32_t completed;
    /// @brief Padding space.
    char align[PAD_CACHE(sizeof(HalfCCSynchNode))];
} CCSynchNode;

/// @brief CCSynchThreadState stores each thread's local state for a single instance of CC-Synch.
/// For each instance of CC-Synch, a discrete instance of CCSynchThreadState should be used.
typedef struct CCSynchThreadState {
    /// @brief pointer to an empty request that would be used for announcing future requests.
    CCSynchNode *next;
    /// @brief A toggle-bit used by the CC-Synch object.
    int toggle;
} CCSynchThreadState;

/// @brief CCSynchStruct stores the state of an instance of the a CC-Synch combining object.
/// CCSynchStruct should be initialized using the CCSynchStructInit function.
typedef struct CCSynchStruct {
    /// @brief Tail points to the most recently announced request. Initially, it points to NULL.
    volatile CCSynchNode *Tail CACHE_ALIGN;
    /// @brief Pointer to the pool of nodes used by threads in order to announce their requests.
    CCSynchNode *nodes CACHE_ALIGN;
    /// @brief The number of threads that will use the CC-Synch combining object.
    uint32_t nthreads;
#ifdef DEBUG_CC
    volatile uint64_t counter CACHE_ALIGN;
    volatile int rounds;
#endif
} CCSynchStruct;

/// @brief This function initializes an instance of the CC-Synch combining object.
///
/// This function should be called once (by a single thread) before any other thread tries to
/// apply any request by using the CCSynchApplyOp function.
///
/// @param l A pointer to an instance of the CC-Synch combining object.
/// @param nthreads The number of threads that will use the CC-Synch combining object.
void CCSynchStructInit(CCSynchStruct *l, uint32_t nthreads);

/// @brief This function should be called once before the thread applies any operation to the CC-Synch combining object.
///
/// @param l A pointer to an instance of the CC-Synch combining object.
/// @param st_thread A pointer to thread's local state of CC-Synch.
/// @param pid The pid of the calling thread.
void CCSynchThreadStateInit(CCSynchStruct *l, CCSynchThreadState *st_thread, int pid);

/// @brief This function is called whenever a thread wants to apply an operation to the simulated concurrent object.
///
/// @param l A pointer to an instance of the CC-Synch combining object.
/// @param st_thread A pointer to thread's local state for a specific instance of CC-Synch.
/// @param sfunc A serial function that the CC-Synch instance should execute, while applying requests announced by active threads.
/// @param state A pointer to the state of the simulated object.
/// @param arg The argument of the request that the thread wants to apply.
/// @param pid The pid of the calling thread.
/// @return RetVal The return value of the applied request.
RetVal CCSynchApplyOp(CCSynchStruct *l, CCSynchThreadState *st_thread, RetVal (*sfunc)(void *, ArgVal, int), void *state, ArgVal arg, int pid);

struct EliminationStack;

typedef struct CCSynchElimStruct {
    /// @brief Tail points to the most recently announced request. Initially, it points to NULL.
    volatile CCSynchNode *Tail CACHE_ALIGN;
    /// @brief Pointer to the pool of nodes used by threads in order to announce their requests.
    CCSynchNode *nodes CACHE_ALIGN;

    EliminationStack *elimination_stack CACHE_ALIGN;

    /// @brief The number of threads that will use the CC-Synch combining object.
    uint32_t nthreads;
#ifdef DEBUG_CC
    volatile uint64_t counter CACHE_ALIGN;
    volatile int rounds;
#endif
} CCSynchElimStruct;

void CCSynchElimThreadStateInit(CCSynchElimStruct *l, CCSynchThreadState *st_thread, int pid);
void CCSynchElimStructInit(CCSynchElimStruct *l, uint32_t nthreads);

RetVal CCSynchApplyEliminatableOp(CCSynchElimStruct *l, CCSynchThreadState *st_thread, RetVal (*sfunc)(void *, ArgVal, int), void *state, ArgVal arg, int pid);

RetVal CCSynchApplyElimNewOp(CCSynchElimStruct *l, CCSynchThreadState *st_thread, RetVal (*sfunc)(void *, ArgVal, int), void *state, ArgVal arg, int pid);

int min(int a, int b);


#include <stdbool.h>
#include <primitives_CC.h>
#include <threadtools_CC.h>
#include <eliminationstack_CC.h>


static const int CCSYNCH_HELP_FACTOR = 10;
#define DEQUEUE_OP INT32_MIN


RetVal CCSynchApplyOp(CCSynchStruct *l, CCSynchThreadState *st_thread, RetVal (*sfunc)(void *, ArgVal, int), void *state, ArgVal arg, int pid) {
    volatile CCSynchNode *p;
    volatile CCSynchNode *cur;
    CCSynchNode *next_node, *tmp_next;
    int help_bound = CCSYNCH_HELP_FACTOR * l->nthreads;
    int counter = 0;

    next_node = st_thread->next;
    next_node->next = NULL;
    next_node->locked = true;
    next_node->completed = false;

    cur = (CCSynchNode *)synchSWAP(&l->Tail, next_node);
    cur->arg_ret = arg;
    cur->pid = pid;
    cur->next = (CCSynchNode *)next_node;
    st_thread->next = (CCSynchNode *)cur;

    while (cur->locked) { // spinning
        synchResched();
    }
    if (cur->completed) // I have been helped
        return cur->arg_ret;
#ifdef DEBUG_CC
    l->rounds++;
#endif
    p = cur; // I am not been helped
    while (p->next != NULL && counter < help_bound) {
        synchStorePrefetch(p->next);
        counter++;
#ifdef DEBUG_CC
        l->counter++;
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
    p->locked = false; // Unlock the next one
    synchStoreFence();

    return cur->arg_ret;
}

void CCSynchStructInit(CCSynchStruct *l, uint32_t nthreads) {
    l->nthreads = nthreads;

    if (synchGetMachineModel() == INTEL_X86_MACHINE) {
        l->nodes = NULL;
        l->Tail = synchGetAlignedMemory(CACHE_LINE_SIZE, sizeof(CCSynchNode));
    } else {
        l->nodes = synchGetAlignedMemory(CACHE_LINE_SIZE, (nthreads + 1) * sizeof(CCSynchNode));
        l->Tail = &l->nodes[nthreads];
    }

#ifdef DEBUG_CC
    l->rounds = l->counter = 0;
#endif

    l->Tail->next = NULL;
    l->Tail->locked = false;
    l->Tail->completed = false;

    synchStoreFence();
}

void CCSynchThreadStateInit(CCSynchStruct *l, CCSynchThreadState *st_thread, int pid) {
    if (synchGetMachineModel() == INTEL_X86_MACHINE) {
        st_thread->next = synchGetAlignedMemory(CACHE_LINE_SIZE, sizeof(CCSynchStruct));
    } else {
        st_thread->next = &l->nodes[pid];
    }
}



RetVal CCSynchApplyEliminatableOp(CCSynchElimStruct *l, CCSynchThreadState *st_thread, RetVal (*sfunc)(void *, ArgVal, int), void *state, ArgVal arg, int pid) {
    volatile CCSynchNode *p;
    volatile CCSynchNode *cur;
    volatile CCSynchNode *topNode;
    CCSynchNode *next_node, *tmp_next;
    int help_bound = CCSYNCH_HELP_FACTOR * l->nthreads;
    int counter = 0;

    next_node = st_thread->next;
    next_node->next = NULL;
    next_node->locked = true;
    next_node->completed = false;

    cur = (CCSynchNode *)synchSWAP(&l->Tail, next_node);
    cur->arg_ret = arg;
    cur->pid = pid;
    cur->next = (CCSynchNode *)next_node;
    st_thread->next = (CCSynchNode *)cur;

    while (cur->locked) { // spinning
        synchResched();
    }
    if (cur->completed) // I have been helped
        return cur->arg_ret;
#ifdef DEBUG_CC
    l->rounds++;
#endif
    p = cur; // I am not been helped
    while (p->next != NULL && counter < help_bound) {
        synchStorePrefetch(p->next);
        counter++;
#ifdef DEBUG_CC
        l->counter++;
#endif
        tmp_next = p->next;
        if (EliminationStackEmpty(l->elimination_stack) == 1) {
            EliminationStackPush(l->elimination_stack, p);
        } else {
            topNode = EliminationStackTop(l->elimination_stack);
            if ((topNode->arg_ret == (ArgVal)DEQUEUE_OP && p->arg_ret != (ArgVal)DEQUEUE_OP) || (topNode->arg_ret != (ArgVal)DEQUEUE_OP && p->arg_ret == (ArgVal)DEQUEUE_OP)) {
                Eliminate(EliminationStackPop(l->elimination_stack), p);
            } else {
                EliminationStackPush(l->elimination_stack, p);
            }
        }
        p = tmp_next;
    }

    while (EliminationStackEmpty(l->elimination_stack) == 0) {
        topNode = EliminationStackPop(l->elimination_stack);
        topNode->arg_ret = sfunc(state, topNode->arg_ret, topNode->pid);
        synchNonTSOFence();
        topNode->completed = true;
        synchNonTSOFence();
        topNode->locked = false;
    }
    synchNonTSOFence();
    p->locked = false;
    synchStoreFence();

    return cur->arg_ret;
}


void CCSynchElimStructInit(CCSynchElimStruct *l, uint32_t nthreads) {
    l->nthreads = nthreads;

    if (synchGetMachineModel() == INTEL_X86_MACHINE) {
        l->nodes = NULL;
        l->Tail = synchGetAlignedMemory(CACHE_LINE_SIZE, sizeof(CCSynchNode));
    } else {
        l->nodes = synchGetAlignedMemory(CACHE_LINE_SIZE, (nthreads + 1) * sizeof(CCSynchNode));
        l->Tail = &l->nodes[nthreads];
    }

#ifdef DEBUG_CC
    l->rounds = l->counter = 0;
#endif

    l->Tail->next = NULL;
    l->Tail->locked = false;
    l->Tail->completed = false;

    l->elimination_stack = EliminationStackAllocate(CCSYNCH_HELP_FACTOR * nthreads);

    synchStoreFence();
}

void CCSynchElimThreadStateInit(CCSynchElimStruct *l, CCSynchThreadState *st_thread, int pid) {
    if (synchGetMachineModel() == INTEL_X86_MACHINE) {
        st_thread->next = synchGetAlignedMemory(CACHE_LINE_SIZE, sizeof(CCSynchElimStruct));
    } else {
        st_thread->next = &l->nodes[pid];
    }
}

int min(int a, int b) {
    return a < b ? a : b;
}


RetVal CCSynchApplyElimNewOp(CCSynchElimStruct *l, CCSynchThreadState *st_thread, RetVal (*sfunc)(void *, ArgVal, int), void *state, ArgVal arg, int pid) {
    volatile CCSynchNode *p;
    volatile CCSynchNode *cur;
    volatile CCSynchNode *elim_ptr;
    volatile CCSynchNode *pred;
    CCSynchNode *next_node, *tmp_next, *tmp_elim;
    int help_bound = CCSYNCH_HELP_FACTOR * l->nthreads;
    int counter = 0;

    next_node = st_thread->next;
    next_node->next = NULL;
    next_node->locked = true;
    next_node->completed = false;

    cur = (CCSynchNode *)synchSWAP(&l->Tail, next_node);
    cur->arg_ret = arg;
    cur->pid = pid;
    cur->next = (CCSynchNode *)next_node;
    st_thread->next = (CCSynchNode *)cur;

    while (cur->locked) { // spinning
        synchResched();
    }
    if (cur->completed) // I have been helped
        return cur->arg_ret;
#ifdef DEBUG_CC
    l->rounds++;
#endif
    p = cur; // I am not been helped
    elim_ptr = cur;
    while (p->next != NULL && counter < help_bound) {
        synchStorePrefetch(p->next);
        counter++;
#ifdef DEBUG_CC
        l->counter++;
#endif
        if (elim_ptr->arg_ret != p->arg_ret && (elim_ptr->arg_ret == DEQUEUE_OP || p->arg_ret == DEQUEUE_OP)) {
            tmp_elim = elim_ptr->next;
            tmp_next = p->next;
            pred->next = tmp_next;  
            if (tmp_elim == p) tmp_elim = tmp_next;
            Eliminate(elim_ptr, p);
            p = tmp_next;
            elim_ptr = tmp_elim;
        } else {
            pred = p;
            p = p->next;
        }
        
    }

    while (elim_ptr != p) {
        tmp_next = elim_ptr->next;
        elim_ptr->arg_ret = sfunc(state, elim_ptr->arg_ret, elim_ptr->pid);
        synchNonTSOFence();
        elim_ptr->completed = true;
        synchNonTSOFence();
        elim_ptr->locked = false;
        elim_ptr = tmp_next;



    }

    synchNonTSOFence();
    p->locked = false; // Unlock the next one
    synchStoreFence();

    return cur->arg_ret;
}


#endif
