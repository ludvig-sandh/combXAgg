/// @file hsynch.h
/// @author Nikolaos D. Kallimanis (nkallima@gmail.com)
/// @brief This file exposes the API of the HSynch combining object.
/// An example of use of this API is provided in benchmarks/hsynchbench.c file.
///
/// For a more detailed description see the original publication:
/// Panagiota Fatourou, and Nikolaos D. Kallimanis."Revisiting the combining synchronization technique".
/// ACM SIGPLAN Notices. Vol. 47. No. 8. ACM, PPoPP 2012.
/// @copyright Copyright (c) 2021
#ifndef _HSYNCH_H_
#define _HSYNCH_H_

#include <config.h>
#include <primitives.h>
#include <clh.h>

/// @brief Whenever numa_regions is equal to HSYNCH_DEFAULT_NUMA_POLICY, the user uses the default number of NUMA nodes,
/// which is equal to the number of NUMA nodes that the machine provides. The information about machine's NUMA 
/// characteristics is provided by the functionality of numa.h lib. In case that numa_regions is different than
/// HSYNCH_DEFAULT_NUMA_POLICY, the user overrides system's default number of NUMA nodes. For example, if  numa_regions = 2
/// and the machine is equipped with 4 NUMA nodes,then the H-Synch will ignore this and will create a fictitious topology of 
/// 2 NUMA nodes. This is very useful in cases of machines that provide many NUMA nodes, but each each of them is equipped
/// with a small amount of cores. In such a case, the combining degree of H-Synch may be restricted. Thus, creating a 
/// fictitious topology with restricted number of NUMA nodes gives much better performance. The user usually overrides 
/// HSYNCH_DEFAULT_NUMA_POLICY by setting the '-n' argument in the executable of the benchmarks.
#define HSYNCH_DEFAULT_NUMA_POLICY 0

/// @brief HalfHSynchNode should not be directly used by the user.
/// It is internally used for proper alignment of the HSynchNode struct.
typedef struct HalfHSynchNode {
    struct HalfHSynchNode *next;
    ArgVal arg_ret;
    uint32_t pid;
    uint32_t locked;
    uint32_t completed;
} HalfHSynchNode;

/// @brief HSynchNode stores the data of an announced request.
typedef struct HSynchNode {
    /// @brief Pointer to the next request that has been announced.
    struct HSynchNode *next;
    /// @brief This variable stores the argument of the request and the return value after the request is applied.
    ArgVal arg_ret;
    /// @brief The pid of the thread that announced this request.
    uint32_t pid;
    /// @brief Whenever it is equal to false, the thread is the combiner; otherwise the thread waits until a combiner apply its request.
    uint32_t locked;
    /// @brief If true, the request is applied and the thread returns its return value.
    uint32_t completed;
    /// @brief Padding space.
    char align[PAD_CACHE(sizeof(HalfHSynchNode))];
} HSynchNode;

/// @brief HSynchNodePtr is a struct for padding pointers to nodes of requests.
typedef union HSynchNodePtr {
    volatile HSynchNode *ptr;
    char pad[CACHE_LINE_SIZE];
} HSynchNodePtr;

/// @brief HSynchThreadState stores each thread's local state for a single instance of HSynch.
/// For each instance of HSynch, a discrete instance of HSynchThreadState should be used.
typedef struct HSynchThreadState {
    /// @brief pointer to an empty request that would be used for announcing future requests.
    HSynchNode *next_node;
} HSynchThreadState;

///  @brief HSynchStruct stores the state of an instance of the a HSynch combining object.
/// HSynchStruct should be initialized using the HSynchStructInit function.
typedef struct HSynchStruct {
    /// @brief A CLH lock is used giving to the threads of each Numa node exclusive access to the object.
    CLHLockStruct *central_lock CACHE_ALIGN;
    /// @brief A tail to the list of announced requests.
    HSynchNodePtr *Tail CACHE_ALIGN;
#ifdef DEBUG
    volatile uint64_t counter CACHE_ALIGN;
    volatile int rounds;
#endif
    /// @brief Pointer to pools of nodes used by threads in order to announce their requests.
    /// HSynch maintains a discrete pool for each Numa node.
    HSynchNode **nodes CACHE_ALIGN;
    /// @brief Used for constructing the Numa topology.
    int32_t *node_indexes;
    /// @brief The number of threads that will use the HSynch combining object.
    uint32_t nthreads;
    /// @brief The size in terms of processing elements that each Numa node has.
    uint32_t numa_node_size;
    /// @brief The number of Numa nodes.
    uint32_t numa_nodes;
    /// @brief The numa policy that the system follows.
    bool numa_policy;
} HSynchStruct;

/// @brief This function initializes an instance of the HSynch combining object.
///
/// This function should be called once (by a single thread) before any other thread tries to
/// apply any request by using the HSynchApplyOp function.
///
/// @param l A pointer to an instance of the HSynch combining object.
/// @param nthreads The number of threads that will use the HSynch combining object.
/// @param numa_regions The number of Numa nodes (which may differ with the actual hw numa nodes) that H-Synch should consider.
/// In case that numa_nodes is equal to HSYNCH_DEFAULT_NUMA_POLICY, the number of Numa nodes provided by the HW is used
/// (see more on hsynch.h).
void HSynchStructInit(HSynchStruct *l, uint32_t nthreads, uint32_t numa_regions);

/// @brief This function should be called once before the thread applies any operation to the HSynch combining object.
///
/// @param l A pointer to an instance of the HSynch combining object.
/// @param st_thread A pointer to thread's local state of HSynch.
/// @param pid The pid of the calling thread.
void HSynchThreadStateInit(HSynchStruct *l, HSynchThreadState *st_thread, int pid);

/// @brief This function is called whenever a thread wants to apply an operation to the simulated concurrent object.
///
/// @param l A pointer to an instance of the HSynch combining object.
/// @param st_thread A pointer to thread's local state for a specific instance of HSynch.
/// @param sfunc A serial function that the HSynch instance should execute, while applying requests announced by active threads.
/// @param state A pointer to the state of the simulated object.
/// @param arg The argument of the request that the thread wants to apply.
/// @param pid The pid of the calling thread.
/// @return RetVal The return value of the applied request.
RetVal HSynchApplyOp(HSynchStruct *l, HSynchThreadState *st_thread, RetVal (*sfunc)(void *, ArgVal, int), void *state, ArgVal arg, int pid);
#endif



#include <stdio.h>
#include <threadtools.h>

#ifdef SYNCH_NUMA_SUPPORT
#    include <numa.h>
#endif

#define HSYNCH_HELP_FACTOR            10
#define HSYNCH_DEFAULT_NUMA_NODE_SIZE 8

static __thread int node_of_thread = 0;

RetVal HSynchApplyOp(HSynchStruct *l, HSynchThreadState *st_thread, RetVal (*sfunc)(void *, ArgVal, int), void *state, ArgVal arg, int pid) {
    volatile HSynchNode *p;
    volatile HSynchNode *cur;
    register HSynchNode *next_node, *tmp_next;
    register int counter = 0;
    int help_bound = HSYNCH_HELP_FACTOR * l->nthreads;

    next_node = st_thread->next_node;
    next_node->next = NULL;
    next_node->locked = true;
    next_node->completed = false;

    cur = (volatile HSynchNode *)synchSWAP(&l->Tail[node_of_thread].ptr, next_node);
    cur->arg_ret = arg;
    cur->pid = pid;
    cur->next = (HSynchNode *)next_node;

    st_thread->next_node = (HSynchNode *)cur;

    while (cur->locked) // spinning
        synchResched();

    p = cur;            // I am not been helped
    if (cur->completed) // I have been helped
        return cur->arg_ret;
    CLHLock(l->central_lock, pid);
#ifdef DEBUG
    l->rounds++;
#endif
    while (counter < help_bound && p->next != NULL) {
        synchReadPrefetch(p->next);
        counter++;
#ifdef DEBUG
        l->counter++;
#endif
        tmp_next = p->next;
        p->arg_ret = sfunc(state, p->arg_ret, p->pid);
        synchNonTSOFence();
        p->completed = true;
        synchNonTSOFence();
        p->locked = false;
        p = tmp_next;

        // A full-memory barrier is inserted for performance optimization, with conditional behavior based on the processor type.
        // This memory barrier is insert to enhance performance in a specific scenario. On non-Intel processors, applying this
        // full-memory barrier can yield a slight performance improvement, when there are no remaining requests to be served.
        // However, it's important to note that on Intel X86 machines, this barrier may actually degrade performance.
        if (tmp_next->next == NULL && synchGetMachineModel() != INTEL_X86_MACHINE)
            synchFullFence();
    }
    p->locked = false; // Unlock the next one
    CLHUnlock(l->central_lock, pid);

    return cur->arg_ret;
}

void HSynchThreadStateInit(HSynchStruct *l, HSynchThreadState *st_thread, int pid) {
    HSynchNode *last_node = NULL;
    uint32_t node_index = 0;

#ifdef SYNCH_NUMA_SUPPORT
    if (l->numa_policy) {
        if (synchGetPreferredCore() != -1) {
            node_of_thread = synchGetPreferredNumaNode();
            if (node_of_thread == -1)
                node_of_thread = pid / l->numa_node_size;
        }
    } else {
        int ncpus = synchGetNCores();
        if (numa_node_of_cpu(0) == numa_node_of_cpu(ncpus / 2) && ncpus > 1) {
            int actual_numa_node = synchGetPreferredNumaNode();
            int actual_per_manual = numa_num_task_nodes() / l->numa_nodes;
            if (actual_per_manual != 0)
                node_of_thread = actual_numa_node / actual_per_manual;
            else {
                int threads_per_node = l->nthreads / l->numa_nodes;
                node_of_thread = synchGetPreferredCore() / threads_per_node;
            }
        } else {
            node_of_thread = pid / l->numa_node_size;
        }
    }
#else
    node_of_thread = pid / l->numa_node_size;
#endif

    if (l->nodes[node_of_thread] == NULL) {
        HSynchNode *ptr = synchGetAlignedMemory(CACHE_LINE_SIZE, (l->numa_node_size + 2) * sizeof(HSynchNode));

        last_node = &ptr[l->numa_node_size + 1];
        last_node->next = NULL;
        last_node->locked = false;
        last_node->completed = false;

        if (synchCASPTR(&l->nodes[node_of_thread], NULL, ptr) == false) 
            synchFreeMemory(ptr, (l->numa_node_size + 2) * sizeof(HSynchNode));
    }
    last_node = l->nodes[node_of_thread] + l->numa_node_size + 1;
    synchCASPTR(&l->Tail[node_of_thread].ptr, NULL, last_node);
    node_index = synchFAA32(&l->node_indexes[node_of_thread], 1);
    st_thread->next_node = l->nodes[node_of_thread] + node_index;
#ifdef DEBUG
    fprintf(stderr, "DEBUG: thread_id: %d -- running_core: %d -- running_node: %d -- hsynch_node: %d\n",
            pid, synchGetPreferredCore(), synchGetPreferredNumaNode(), node_of_thread);
#endif
}

void HSynchStructInit(HSynchStruct *l, uint32_t nthreads, uint32_t numa_regions) {
    int i;

    if (numa_regions > nthreads)
        numa_regions = nthreads;
    l->nthreads = nthreads;
    if (numa_regions == HSYNCH_DEFAULT_NUMA_POLICY) {
        // Whenever numa_regions is equal to HSYNCH_DEFAULT_NUMA_POLICY, the user uses
        // the default number of NUMA nodes, which is equal to the number of NUMA nodes
        // that the machine provides. The information about machine's NUMA characteristics
        // is provided by the functionality of numa.h lib.
        // In case that numa_regions is different than HSYNCH_DEFAULT_NUMA_POLICY, the
        // user overrides system's default number of NUMA nodes. For example, if 
        // numa_regions = 2 and the machine is equipped with 4 NUMA nodes,then the
        // H-Synch will ignore this and will create a fictitious topology of 2 NUMA nodes. 
        // This is very useful in cases of machines that provide many NUMA nodes,
        // but each each of them is equipped with a small amount of cores. In such a 
        // case, the combining degree of H-Synch may be restricted. Thus, creating
        // a fictitious topology with restricted number of NUMA nodes gives much
        // better performance. The user usually overrides HSYNCH_DEFAULT_NUMA_POLICY
        // by setting the '-n' argument in the executable of the benchmarks.
        l->numa_policy = true;

#ifdef SYNCH_NUMA_SUPPORT
        uint32_t ncpus = synchGetNCores();

        l->numa_nodes = numa_num_task_nodes();
        l->numa_node_size = nthreads / l->numa_nodes + (nthreads % l->numa_nodes);

        if (l->numa_node_size < ncpus / l->numa_nodes)
            l->numa_node_size = ncpus / l->numa_nodes;
#else
        l->numa_node_size = HSYNCH_DEFAULT_NUMA_NODE_SIZE;
        l->numa_nodes = nthreads / l->numa_node_size + (nthreads % l->numa_node_size == 0 ? 0 : 1);
        if (l->numa_nodes == 0)
            l->numa_nodes = 1;
#endif

    } else {
        l->numa_policy = false;
        l->numa_nodes = numa_regions;
        l->numa_node_size = nthreads / l->numa_nodes;
        if (nthreads % l->numa_nodes != 0) 
            l->numa_node_size *= 2;
    }

    l->central_lock = CLHLockInit(nthreads);
    l->nodes = synchGetAlignedMemory(CACHE_LINE_SIZE, l->numa_nodes * sizeof(HSynchNode *));
    l->Tail = synchGetAlignedMemory(CACHE_LINE_SIZE, l->numa_nodes * sizeof(HSynchNodePtr));
    l->node_indexes = synchGetAlignedMemory(CACHE_LINE_SIZE, l->numa_nodes * sizeof(uint32_t));
    for (i = 0; i < l->numa_nodes; i++) {
        l->node_indexes[i] = 0;
        l->nodes[i] = NULL;
        l->Tail[i].ptr = NULL;
    }
#ifdef DEBUG
    l->rounds = l->counter = 0;
#endif
    synchStoreFence();
}

