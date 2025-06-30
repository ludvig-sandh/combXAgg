/// @file threadtools.h
/// @brief This file exposes a simple API for handling both posix and user-level threads.
/// This API provides functionality for creating new threads (both posix and user-level),
/// functionality for setting affinities, functionality for yielding the processor, etc.
/// Examples of usage could be found in almost all the provided benchmarks under the benchmarks directory.
#ifndef _THREAD_H_
#define _THREAD_H_

#include <config.h>
#include <stdint.h>
#include <stdbool.h>

#define SYNCH_DONT_USE_UTHREADS                       1

/// @brief Threads are distributed in a round-robin fashion across all processing cores.
#define SYNCH_THREAD_PLACEMENT_FLAT                   0x1

/// @brief It optimizes thread placement for systems with Non-Uniform Memory Access (NUMA) by spreading threads sparsely 
/// across NUMA nodes, potentially improving memory bandwidth and improving cache utilization.
#define SYNCH_THREAD_PLACEMENT_NUMA_SPARSE            0x2

/// @brief It places threads within the smallest number of NUMA nodes before spreading them to other nodes,
/// which can improve memory locality and may reduce contention on shared variables.
#define SYNCH_THREAD_PLACEMENT_NUMA_DENSE             0x3

/// @brief Similar to `SYNCH_THREAD_PLACEMENT_NUMA_DENSE`, but with a preference for utilizing Simultaneous Multithreading (SMT)
/// capabilities within NUMA nodes to maximize processing efficiency.
#define SYNCH_THREAD_PLACEMENT_NUMA_SPARSE_SMT_PREFER 0x4

/// @brief It combines the sparse distribution strategy across NUMA nodes with a preference for SMT.
/// This policy spreads threads across NUMA nodes to avoid contention, while preferring to fill SMT slots within each core before moving to
/// the next. It aims to strike a balance between improving memory bandwidth and leveraging SMT for higher processing efficiency and reduced
/// contention on shared variables.
#define SYNCH_THREAD_PLACEMENT_NUMA_DENSE_SMT_PREFER  0x5

/// @brief The maximum defined supported thread placement policy that is available.
#define SYNCH_THREAD_PLACEMENT_POLICY_MAX             SYNCH_THREAD_PLACEMENT_NUMA_DENSE_SMT_PREFER

/// @brief By default the thread placement policy is se to `SYNCH_THREAD_PLACEMENT_DEFAULT`.
/// Currently, `SYNCH_THREAD_PLACEMENT_DEFAULT` is equal to `SYNCH_THREAD_PLACEMENT_NUMA_SPARSE_SMT_PREFER`.
#define SYNCH_THREAD_PLACEMENT_DEFAULT                SYNCH_THREAD_PLACEMENT_NUMA_SPARSE_SMT_PREFER

/// @brief This function creates nthreads posix threads, where each posix thread executes
/// uthreads user-level threads (fibers). Thus, the total amount of threads and fibers is nthreads * uthreads.
/// Each of the created threads executes the func function, the function has as an argument
/// the id of the created thread, which is a unique integer in {0, ..., nthreads * uthreads - 1}.
/// In case, the user does not want to create any fiber, uthreads should be equal to SYNCH_DONT_USE_UTHREADS.
/// @param nthreads The number of posix threads.
/// @param func A function that each fiber and posix thread should execute. This function has
/// as a single argument the unique id of the thread.
/// @param uthreads The number of fibers that each posix thread should execute.
int synchStartThreadsN(uint32_t nthreads, void *(*func)(void *), uint32_t uthreads);

/// @brief This function returns whenever all the created posix threads and fibers spawned by StartThreadsN
/// have completed the execution. 
/// @param nthreads The number of posix threads that StartThreadsN spawned.
void synchJoinThreadsN(uint32_t nthreads);

///@brief This function sets the default placement policy of threads in machine's processors. 
/// The thread placement policy could set any of the following:
/// - `SYNCH_THREAD_PLACEMENT_FLAT`: Threads are distributed in a round-robin fashion across all processing cores.
/// - `SYNCH_THREAD_PLACEMENT_NUMA_SPARSE`: Optimizes thread placement for systems with Non-Uniform Memory Access (NUMA)
/// by spreading threads sparsely across NUMA nodes, potentially improving memory bandwidth and improving cache utilization.
/// - `SYNCH_THREAD_PLACEMENT_NUMA_DENSE`: Places threads within the smallest number of NUMA nodes before spreading them to other nodes,
/// which can improve memory locality and may reduce contention on shared variables.
/// - `SYNCH_THREAD_PLACEMENT_NUMA_DENSE_SMT_PREFER`: Similar to `SYNCH_THREAD_PLACEMENT_NUMA_DENSE`, but with a preference 
/// for utilizing Simultaneous Multithreading (SMT) capabilities within NUMA nodes to maximize processing efficiency.
/// - `SYNCH_THREAD_PLACEMENT_NUMA_SPARSE_SMT_PREFER`: Combines the sparse distribution strategy across NUMA nodes with a preference for SMT.
/// This policy spreads threads across NUMA nodes to avoid contention, while preferring to fill SMT slots within each core before moving to
/// the next. It aims to strike a balance between improving memory bandwidth and leveraging SMT for higher processing efficiency and reduced
/// contention on shared variables.
/// - `SYNCH_THREAD_PLACEMENT_DEFAULT`: By default the thread placement policy is se to `SYNCH_THREAD_PLACEMENT_DEFAULT`.
/// Currently, `SYNCH_THREAD_PLACEMENT_DEFAULT` is equal to `SYNCH_THREAD_PLACEMENT_NUMA_SPARSE_SMT_PREFER`.
/// @param policy The thread placement policy to be set.
void synchSetThreadPlacementPolicy(uint32_t policy);

/// @brief Retrieves the current thread placement policy for the machine's processors.
/// This function returns the policy setting that determines how threads are distributed across the processing cores of the machine.
/// The possible return values correspond to the thread placement policies that could be returned are the following:
/// - `SYNCH_THREAD_PLACEMENT_FLAT`: Threads are distributed in a round-robin fashion across all processing cores.
/// - `SYNCH_THREAD_PLACEMENT_NUMA_SPARSE`: Optimizes thread placement for systems with Non-Uniform Memory Access (NUMA)
/// by spreading threads sparsely across NUMA nodes, potentially improving memory bandwidth and improving cache utilization.
/// - `SYNCH_THREAD_PLACEMENT_NUMA_DENSE`: Places threads within the smallest number of NUMA nodes before spreading them to other nodes,
/// which can improve memory locality and may reduce contention on shared variables.
/// - `SYNCH_THREAD_PLACEMENT_NUMA_DENSE_SMT_PREFER`: Similar to `SYNCH_THREAD_PLACEMENT_NUMA_DENSE`, but with a preference 
/// for utilizing Simultaneous Multithreading (SMT) capabilities within NUMA nodes to maximize processing efficiency.
/// - `SYNCH_THREAD_PLACEMENT_NUMA_SPARSE_SMT_PREFER`: Combines the sparse distribution strategy across NUMA nodes with a preference for SMT.
/// This policy spreads threads across NUMA nodes to avoid contention, while preferring to fill SMT slots within each core before moving to
/// the next. It aims to strike a balance between improving memory bandwidth and leveraging SMT for higher processing efficiency and reduced
/// contention on shared variables.
/// - `SYNCH_THREAD_PLACEMENT_DEFAULT`: By default the thread placement policy is se to `SYNCH_THREAD_PLACEMENT_DEFAULT`.
/// Currently, `SYNCH_THREAD_PLACEMENT_DEFAULT` is equal to `SYNCH_THREAD_PLACEMENT_NUMA_SPARSE_SMT_PREFER`.
uint32_t synchGetThreadPlacementPolicy(void);

/// @brief This function sets the CPU affinity of the running thread to cpu_id, where cpu_id
/// should be a unique integer in {0, ..., N-1}, where N is the amount of available processing cores.
int synchThreadPin(int32_t cpu_id);

inline uint32_t synchPreferredNumaNodeOfThread(uint32_t pid);

/// @brief This function returns the id of the running thread (posix or fiber). More specifically, it returns
/// a unique integer in {0, ..., N-1}, where N is the amount of the running threads. For example, if 3 Posix threads
/// are running, and 4 fiber threads are running inside each Posix thread, this function will return an integer
/// in the interval of {0, ...., 11}.
inline int32_t synchGetThreadId(void);

inline int32_t synchGetPreferredNumaNode(void);

/// @brief This fuction returns the id of the current posix thread. 
/// This function should return an identical value for any fiber running in the same posix thread.
inline int32_t synchGetPosixThreadId(void);

/// @brief This function returns the core-id of the current posix thread or fiber. The core-id is a
/// unique integer in {0, ..., N-1}, where N is the amount of available processing cores.
inline int32_t synchGetPreferredCore(void);

/// @brief This function returns the core-id of the posix thread or fiber with id equal to pid. 
/// The core-id is a unique integer in {0, ..., N-1}, where N is the amount of available processing cores.
inline uint32_t synchPreferredCoreOfThread(uint32_t pid);

/// @brief This function returns the number of system's processing cores.
inline uint32_t synchGetNCores(void);

/// @brief In case that this function is called by a posix thread, it hints OS to give the CPU to
/// some other thread. In case that this function is called by a fiber, it gives the CPU control 
/// to the next fiber (if any) running in the same posix thread.
inline void synchResched(void);

/// @brief This function returns true if the number of spawned threads is greater than the number of 
/// system's available processing cores; otherwise, this function returns false.
inline bool synchIsSystemOversubscribed(void);




#define _GNU_SOURCE
#include <unistd.h>

// #include <config.h>
// #include <threadtools.h>
#include "primitives.h"
#include <uthreads.h>
#include <barrier.h>
#include <sched.h> // CPU_SET, CPU_ZERO, cpu_set_t, sched_setaffinity()
#include <pthread.h>
#include <stdio.h>

#ifdef SYNCH_NUMA_SUPPORT
#    include <numa.h>
#endif

inline static void *uthreadWrapper(void *arg);
inline static void *kthreadWrapper(void *arg);

static __thread pthread_t *__threads;
static __thread int32_t __thread_id = 0;
static __thread int32_t __preferred_numa_node = 0;
static __thread int32_t __preferred_core = 0;
static __thread int32_t __unjoined_threads = 0;

static void *(*__func)(void *) CACHE_ALIGN = NULL;
static uint32_t __uthreads = 0;
static uint32_t __nthreads = 0;
static uint32_t __ncores = 0;
static uint32_t __schedule_policy = SYNCH_THREAD_PLACEMENT_DEFAULT;
static bool __uthread_sched = false;
static bool __system_oversubscription = false;
static bool __noop_resched = false;
static SynchBarrier bar CACHE_ALIGN;


void synchSetThreadPlacementPolicy(uint32_t policy) {
    if (policy > SYNCH_THREAD_PLACEMENT_POLICY_MAX)
        policy = SYNCH_THREAD_PLACEMENT_POLICY_MAX;
    __schedule_policy = policy;
    synchFullFence();
}

uint32_t synchGetThreadPlacementPolicy(void) {
    return __schedule_policy;
}

void setThreadId(int32_t id) {
    __thread_id = id;
}

inline int32_t synchGetPreferredCore(void) {
    return __preferred_core;
}

inline int32_t synchGetPreferredNumaNode(void) {
    return __preferred_numa_node;
}

inline uint32_t synchGetNCores(void) {
    if (__ncores == 0)
        __ncores = sysconf(_SC_NPROCESSORS_ONLN);
    return __ncores;
}

inline static void *kthreadWrapper(void *arg) {
    int cpu_id;
    long pid = (long)arg;

    cpu_id = pid % synchGetNCores();
    synchThreadPin(cpu_id);
    setThreadId(pid);
    // synchStartCPUCounters(pid); //AJ FIXME
    __func((void *)pid);
    // synchStopCPUCounters(pid); //AJ FIXME
    synchBarrierLeave(&bar);
    return NULL;
}

inline uint32_t synchPreferredCoreOfThread(uint32_t pid) {
    uint32_t preferred_core = 0;

    if (__schedule_policy == SYNCH_THREAD_PLACEMENT_FLAT) {
        preferred_core = pid;
    } else {
#ifdef SYNCH_NUMA_SUPPORT
        uint32_t ncpus = synchGetNCores();
        uint32_t nodes = numa_num_task_nodes();
        uint32_t node_size = ncpus / nodes;

        if (__schedule_policy == SYNCH_THREAD_PLACEMENT_NUMA_SPARSE) {
            if (numa_node_of_cpu(0) == numa_node_of_cpu(ncpus / 2)) { // SMT or HyperThreading detected
                uint32_t half_node_size = node_size / 2;
                uint32_t offset = 0;
                uint32_t half_cpu_id = pid;

                if (pid >= ncpus / 2) {
                    half_cpu_id = pid - ncpus / 2;
                    offset = ncpus / 2;
                }
                preferred_core = (half_cpu_id % nodes) * half_node_size + half_cpu_id / nodes;
                preferred_core += offset;
            } else preferred_core = (pid / nodes) + (pid % nodes) * (node_size);
        } else if (__schedule_policy == SYNCH_THREAD_PLACEMENT_NUMA_SPARSE_SMT_PREFER) {
            if (numa_node_of_cpu(0) == numa_node_of_cpu(ncpus / 2)) { // SMT or HyperThreading detected
                uint32_t double_nodes = 2 * nodes;
                uint32_t half_node_size = node_size / 2;

                preferred_core = (pid % node_size) * half_node_size + (pid / double_nodes);
            } else preferred_core = (pid / nodes) + (pid % nodes) * (node_size);
        } else if (__schedule_policy == SYNCH_THREAD_PLACEMENT_NUMA_DENSE) {
            preferred_core = pid;
        } else if (__schedule_policy == SYNCH_THREAD_PLACEMENT_NUMA_DENSE_SMT_PREFER){
            preferred_core = (pid / nodes) + (pid % nodes) * (node_size);
        } else {
            fprintf(stderr, "ERROR: Unsupported scheduling policy: 0x%X\n", __schedule_policy);
            preferred_core = (pid / nodes) + (pid % nodes) * (node_size);
        }
#else
        preferred_core = pid;
#endif
    }
    preferred_core %= synchGetNCores();

    return preferred_core;
}

inline uint32_t synchPreferredNumaNodeOfThread(uint32_t pid) {
    uint32_t preferred_node = 0;

#ifdef SYNCH_NUMA_SUPPORT
    uint32_t preferred_core = synchPreferredCoreOfThread(pid);
    uint32_t ncpus = synchGetNCores();
    uint32_t nodes = numa_num_task_nodes();
    uint32_t node_size = ncpus / nodes;

    if (numa_node_of_cpu(0) == numa_node_of_cpu(ncpus / 2)) { // SMT or HyperThreading detected
        uint32_t half_node_size = node_size / 2;

        if (preferred_core < ncpus / 2)
            preferred_node = preferred_core/half_node_size;
        else 
            preferred_node = (preferred_core - (ncpus/2))/half_node_size;
    } else {
        preferred_node = preferred_core/node_size;
    }
#else
    preferred_node = 0;
#endif

    return preferred_node;
}

int synchThreadPin(int32_t cpu_id) {
    int ret = 0;
    cpu_set_t mask;
    unsigned int len = sizeof(mask);

    pthread_setconcurrency(synchGetNCores());
    CPU_ZERO(&mask);
    __preferred_core = synchPreferredCoreOfThread(cpu_id);
    __preferred_numa_node = synchPreferredNumaNodeOfThread(cpu_id);
    CPU_SET(__preferred_core, &mask);
#if defined(DEBUG_SH) && defined(SYNCH_NUMA_SUPPORT)
    fprintf(stderr, "DEBUG_SH: posix_thread: %d -- numa_node: %d -- core: %d\n", cpu_id, __preferred_numa_node, __preferred_core);
#endif
    ret = sched_setaffinity(0, len, &mask);
    if (ret == -1)
        perror("sched_setaffinity");

    return ret;
}

inline static void *uthreadWrapper(void *arg) {
    int i, kernel_id;
    long pid = (long)arg;

    kernel_id = (pid / __uthreads) % synchGetNCores();
    synchThreadPin(kernel_id);
    setThreadId(pid);
    // synchStartCPUCounters(kernel_id); //FIXME
    synchInitFibers(__uthreads);
    for (i = 0; i < __uthreads - 1; i++) {
        synchSpawnFiber(__func, pid + i + 1);
#if defined(DEBUG_SH)
        fprintf(stderr, "DEBUG_SH: fiber: %ld\n", pid + i + 1);
#endif
    }
#if defined(DEBUG_SH)
    fprintf(stderr, "DEBUG_SH: fiber: %ld\n", pid);
#endif
    __func((void *)pid);

    synchWaitForAllFibers();
    // synchStopCPUCounters(kernel_id); //FIXME
    synchBarrierLeave(&bar);
    return NULL;
}

int synchStartThreadsN(uint32_t nthreads, void *(*func)(void *), uint32_t uthreads) {
    long i;
    int last_thread_id = -1;

    // synchInitCPUCounters(); // FIXME
    __ncores = sysconf(_SC_NPROCESSORS_ONLN);
    __nthreads = nthreads;
    __threads = synchGetMemory(nthreads * sizeof(pthread_t));
    __func = func;
    synchStoreFence();
    if (uthreads != SYNCH_DONT_USE_UTHREADS && uthreads > 1) {
        __uthreads = uthreads;
        __uthread_sched = true;
        __system_oversubscription = true;
        synchBarrierSet(&bar, nthreads / uthreads + 1);
        for (i = 0; i < (nthreads / uthreads) - 1; i++) {
            last_thread_id = pthread_create(&__threads[i], NULL, uthreadWrapper, (void *)(i * uthreads));
            if (last_thread_id != 0) {
                perror("pthread_create");
                exit(EXIT_FAILURE);
            }
            __unjoined_threads++;
        }
        uthreadWrapper((void *)(i * uthreads));
    } else {
        __uthread_sched = false;
        if (__nthreads > __ncores)
            __system_oversubscription = true;
        else
            __noop_resched = true;
        synchBarrierSet(&bar, nthreads + 1);
        for (i = 0; i < nthreads - 1; i++) {
            last_thread_id = pthread_create(&__threads[i], NULL, kthreadWrapper, (void *)i);
            if (last_thread_id != 0) {
                perror("pthread_create");
                exit(EXIT_FAILURE);
            }
            __unjoined_threads++;
        }
        kthreadWrapper((void *)i);
    }
    return last_thread_id;
}

void synchJoinThreadsN(uint32_t nthreads) {
    synchBarrierLastLeave(&bar);
    synchFreeMemory(__threads, nthreads * sizeof(pthread_t));
}

inline int32_t synchGetThreadId(void) {
    return __thread_id + synchCurrentFiberIndex();
}

inline int32_t synchGetPosixThreadId(void) {
    return __thread_id;
}

inline void synchResched(void) {
    if (__noop_resched) {
        synchPause();
    } else if (__uthread_sched) {
        synchFiberYield();
    } else {
        sched_yield();
    }
}

inline bool synchIsSystemOversubscribed(void) {
    return __system_oversubscription;
}




















#endif
