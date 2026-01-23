/// @file fc.h
/// @author Nikolaos D. Kallimanis (nkallima@gmail.com)
/// @brief This file exposes the API of the flat-combining synchronization technique (or shortly FC).
/// This code is an alternative implementation of flat-combining provided by the Synch framework and it has NO relation with the original code of flat-combining provided in
/// https://github.com/mit-carbon/Flat-Combining, since it is written from the scratch.
/// In many cases, this code performs much better than the original flat-combining implementation. An example of use of this API is provided in benchmarks/fcbench.c file.
/// Stack and queue implementations based on this implementation of flat-combining are provided (see fcstack.c and fcqueue.c).
///
/// For a more detailed description of flat-combining, see the original publication:
/// Danny Hendler, Itai Incze, Nir Shavit, and Moran Tzafrir. Flat combining and the synchronization-parallelism tradeoff. 
/// In Proceedings of the twenty-second annual ACM symposium on Parallelism in algorithms and architectures (SPAA 2010), pp. 355-364.
/// @copyright Copyright (c) 2021
#ifndef _FC_H_
#define _FC_H_

#include <stdint.h>

#include <config_FC.h>
#include <primitives_FC.h>
#include <system_FC.h>

/// @brief HalfFCRequest should not be directly used by the user.
/// It is internally used for proper alignment of the FCRequest struct.
typedef struct HalfFCRequest {
    volatile struct FCRequest *next;
    volatile ArgVal val;
    volatile int age;
    volatile bool active;
    volatile bool pending;
} HalfFCRequest;

typedef struct FCRequest {
    /// @brief A pointer to the next announced request.
    volatile struct FCRequest *next;
    /// @brief This variable stores the argument of the request and the return value after the request is applied.
    volatile ArgVal val;
    /// @brief The age of the current announcement record.
    volatile int age;
    /// @brief If the request is active or not.
    volatile bool active;
    /// @brief If the request is pending or not.
    volatile bool pending;
    /// @brief Padding space.
    char pad[CACHE_LINE_SIZE - sizeof(HalfFCRequest)];
} FCRequest;

/// @brief FCStruct stores the state of an instance of the a FC object.
/// FCStruct should be initialized using the FCStructInit function.
typedef struct FCStruct {
    /// @brief A pointer to the head request.
    volatile struct FCRequest *head CACHE_ALIGN;
    /// @brief A central lock.
    volatile uint64_t lock CACHE_ALIGN;
    /// @brief The array of requests, one per thread.
    FCRequest *nodes;
    volatile uint64_t count CACHE_ALIGN;
    /// @brief This field counts the applied requests.
    volatile uint64_t counter;
    /// @brief The total number of executed combining rounds.
    volatile uint64_t rounds;
} FCStruct;

/// @brief FCThreadState stores each thread's local state for a single instance of FC.
/// For each instance of FC, a discrete instance of FCThreadState should be used.
typedef struct FCThreadState {
    FCRequest *node;
} FCThreadState;

/// @brief This function initializes an instance of the FC object.
///
/// This function should be called once (by a single thread) before any other thread tries to
/// apply any request by using the FCApplyOp function.
///
/// @param l A pointer to an instance of the FC object.
/// @param nthreads The number of threads that will use the FC object.
void FCStructInit(FCStruct *l, uint32_t nthreads);

/// @brief This function should be called once before the thread applies any operation to the FC object.
///
/// @param l A pointer to an instance of the FC object.
/// @param st_thread A pointer to thread's local state of FC.
/// @param pid The pid of the calling thread.
void FCThreadStateInit(FCStruct *l, FCThreadState *st_thread, int pid);

/// @brief This function is called whenever a thread wants to apply an operation to the simulated concurrent object.
///
/// @param l A pointer to an instance of the FC object.
/// @param st_thread A pointer to thread's local state for a specific instance of FC.
/// @param sfunc A serial function that the FC instance should execute, while applying requests announced by active threads.
/// @param state A pointer to the state of the simulated object.
/// @param arg The argument of the request that the thread wants to apply.
/// @param pid The pid of the calling thread.
/// @return RetVal The return value of the applied request.
RetVal FCApplyOp(FCStruct *l, FCThreadState *st_thread, RetVal (*sfunc)(void *, ArgVal, int), void *state, ArgVal arg, int pid);


#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdint.h>

#include <threadtools_FC.h>

#define FC_CLEANUP_FREQUENCY     10
#define FC_CLEANUP_OLD_THRESHOLD 10
#define FC_COMBINING_ROUNDS      3

static void FCEnqueueRequest(FCStruct *lock, FCThreadState *st_thread);

void FCStructInit(FCStruct *l, uint32_t nthreads) {
    l->lock = 0;
    l->count = 0;
    l->head = NULL;
    l->counter = 0;
    l->rounds = 0;
    l->nodes = reinterpret_cast<FCRequest*>(
        synchGetAlignedMemory(CACHE_LINE_SIZE, nthreads * sizeof(FCRequest)));
    synchStoreFence();
}

void FCThreadStateInit(FCStruct *l, FCThreadState *st_thread, int pid) {
    st_thread->node = &l->nodes[pid];
    st_thread->node->age = 0;
    st_thread->node->active = false;
    synchNonTSOFence();
}

static void FCEnqueueRequest(FCStruct *lock, FCThreadState *st_thread) {
    FCRequest *request = st_thread->node;
    FCRequest *supposed;

    request->active = true;
    synchNonTSOFence();

    do {
        synchResched();
        supposed = (FCRequest *)lock->head;
        synchNonTSOFence();
        request->next = supposed;
    } while (!synchCASPTR(&lock->head, supposed, request));
}

RetVal FCApplyOp(FCStruct *lock, FCThreadState *st_thread, RetVal (*sfunc)(void *, ArgVal, int), void *state, ArgVal arg, int pid) {
    struct FCRequest *request;
    int i;

    request = st_thread->node;
    request->val = arg;
    synchNonTSOFence();
    request->pending = true;
    synchStoreFence();
    while (true) {

        if (lock->lock == 0 && synchCAS64(&lock->lock, 0, 1)) {

            break;
        } else {

            while (lock->lock && request->pending && request->active) {
                synchResched();
            }
            if (request->pending == false) {
                return request->val;
            } else if (request->active == false) {
                FCEnqueueRequest(lock, st_thread);
            }
        }
    }


    if (request->active == false) FCEnqueueRequest(lock, st_thread);
    lock->count = 1;
    int count = lock->count;
    volatile FCRequest *cur;


#ifdef DEBUG_FC_STACK
    lock->rounds += 1;
#endif
    for (i = 0; i < FC_COMBINING_ROUNDS; i++) {
        for (cur = lock->head; cur != NULL; cur = cur->next) {
            if (cur->pending) {
                cur->val = sfunc(state, cur->val, pid);
                synchNonTSOFence();
                cur->pending = false;
                cur->age = count;
#ifdef DEBUG_FC_STACK
                lock->counter += 1;
#endif
                synchNonTSOFence();
            }
        }
    }

    if (!(count % FC_CLEANUP_FREQUENCY)) {
        volatile FCRequest *prev = (FCRequest *)lock->head;
        while ((cur = prev->next)) {
            if ((cur->age + FC_CLEANUP_OLD_THRESHOLD) < count) {
                prev->next = cur->next;
                synchNonTSOFence();
                cur->active = 0;
                synchNonTSOFence();
            } else
                prev = cur;
        }
    }
    lock->lock = 0;
    synchStoreFence();

    return request->val;
}


#endif