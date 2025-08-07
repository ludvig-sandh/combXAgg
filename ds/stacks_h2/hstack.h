/// @file hstack.h
/// @author Nikolaos D. Kallimanis (nkallima@gmail.com)
/// @brief This file exposes the API of the H-Stack concurrent stack implementation.
/// An example of use of this API is provided in benchmarks/hstackbench.c file.
///
/// For a more detailed description see the original publication:
/// Panagiota Fatourou, and Nikolaos D. Kallimanis."Revisiting the combining synchronization technique".
/// ACM SIGPLAN Notices. Vol. 47. No. 8. ACM, PPoPP 2012.
/// @copyright Copyright (c) 2021
#ifndef _HSTACK_H_
#define _HSTACK_H_

#include <config.h>
#include <queue-stack.h>
#include <hsynch.h>
#include "primitives.h"

/// @brief HStackStruct stores the state of an instance of the H-Stack concurrent stack implementation.
/// HStackStruct should be initialized using the HStackStructInit function.
typedef struct HStackStruct {
    /// @brief A H-Synch instance for servicing both push and pop operations.
    HSynchStruct object_struct CACHE_ALIGN;
    /// @brief A pointer to the top element of the stack.
    volatile Node *top CACHE_ALIGN;
} HStackStruct;

/// @brief HStackThreadState stores each thread's local state for a single instance of H-Stack.
/// For each instance of H-Stack, a discrete instance of HStackThreadState should be used.
typedef struct HStackThreadState {
    /// @brief An HSynchThreadState struct for the instance of H-Synch that serves both push and pop operations.
    HSynchThreadState th_state;
} HStackThreadState;

/// @brief This function initializes an instance of the H-Stack concurrent stack implementation.
///
/// This function should be called once (by a single thread) before any other thread tries to
/// apply any push or pop operation.
///
/// @param stack_object_struct A pointer to an instance of the H-Stack concurrent stack implementation.
/// @param nthreads The number of threads that will use the H-Stack concurrent stack implementation.
/// @param numa_nodes The number of Numa nodes (which may differ with the actual hw numa nodes) that H-Stack should consider.
/// In case that numa_nodes is equal to HSYNCH_DEFAULT_NUMA_POLICY, the number of Numa nodes provided by the HW is used
/// (see more on hsynch.h).
void HStackInit(HStackStruct *stack_object_struct, uint32_t nthreads, uint32_t numa_nodes);

/// @brief This function should be called once before the thread applies any operation to the H-Stack concurrent stack implementation.
///
/// @param object_struct A pointer to an instance of the H-Stack concurrent stack implementation.
/// @param lobject_struct A pointer to thread's local state of H-Stack.
/// @param pid The pid of the calling thread.
void HStackThreadStateInit(HStackStruct *object_struct, HStackThreadState *lobject_struct, int pid);

/// @brief This function adds (i.e. pushes) a new element to the top of the stack.
/// This element has a value equal with arg.
///
/// @param object_struct A pointer to an instance of the H-Stack concurrent stack implementation.
/// @param lobject_struct A pointer to thread's local state of H-Stack.
/// @param arg The push operation will insert a new element to the stack with value equal to arg.
/// @param pid The pid of the calling thread.
void HStackPush(HStackStruct *object_struct, HStackThreadState *lobject_struct, ArgVal arg, int pid);

/// @brief This function removes (i.e. pops) an element from the top of the stack and returns its value.
///
/// @param object_struct A pointer to an instance of the H-Stack concurrent stack implementation.
/// @param lobject_struct A pointer to thread's local state of H-Stack.
/// @param pid The pid of the calling thread.
/// @return The value of the removed element.
RetVal HStackPop(HStackStruct *object_struct, HStackThreadState *lobject_struct, int pid);





// #include <hstack.h>
#include <pool.h>

inline static RetVal serialPushPop(void *state, ArgVal arg, int pid);

static const int POP_OP = INT_MIN;
static __thread SynchPoolStruct pool_node CACHE_ALIGN;

void HStackInit(HStackStruct *stack_object_struct, uint32_t nthreads, uint32_t numa_nodes) {
    COUTATOMIC("HStackInit: Initializing H-Stack with " << nthreads << " threads and " << numa_nodes << " NUMA nodes." << std::endl);
    HSynchStructInit(&stack_object_struct->object_struct, nthreads, numa_nodes);
    stack_object_struct->top = NULL;
}

void HStackThreadStateInit(HStackStruct *object_struct, HStackThreadState *lobject_struct, int pid) {
    HSynchThreadStateInit(&object_struct->object_struct, &lobject_struct->th_state, (int)pid);
    synchInitPool(&pool_node, sizeof(Node));
}

inline RetVal serialPushPop(void *state, ArgVal arg, int pid) {
    if (arg == POP_OP) {
        volatile HStackStruct *st = (HStackStruct *)state;
        volatile Node *node = st->top;

        if (st->top == NULL) {
            return EMPTY_STACK;
        } else {
            RetVal ret = node->val;
            st->top = st->top->next;
            synchNonTSOFence();
            synchRecycleObj(&pool_node, (void *)node);
            return ret;
        }
    } else {
        HStackStruct *st = (HStackStruct *)state;
        Node *node;

        node = synchAllocObj(&pool_node);
        node->next = st->top;
        node->val = arg;
        st->top = node;
        synchNonTSOFence();
        return PUSH_SUCCESS;
    }
}

void HStackPush(HStackStruct *object_struct, HStackThreadState *lobject_struct, ArgVal arg, int pid) {
    HSynchApplyOp(&object_struct->object_struct, &lobject_struct->th_state, serialPushPop, object_struct, (ArgVal)arg, pid);
}

RetVal HStackPop(HStackStruct *object_struct, HStackThreadState *lobject_struct, int pid) {
    return HSynchApplyOp(&object_struct->object_struct, &lobject_struct->th_state, serialPushPop, object_struct, (ArgVal)POP_OP, pid);
}



















#endif
