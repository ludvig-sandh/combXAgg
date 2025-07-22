/// @file stats.h
/// @brief This file exposes the API for keeping statistics in the provided data-structures.
/// The provided statistics are the total number of atomic instructions, the amount of each 
/// type of atomic instructions, atomic instructions per operation, etc.
/// Notice that this API should be used only by Posix threads. In the case where there more 
/// than one fiber per Posix thread, only a single fiber thread should use this API.
/// In case that the API of threadtools.h is used, most the provided functionality 
/// (except printStats function), should not be directly used by the user.
#ifndef _STATS_H_
#define _STATS_H_

#include <stdint.h>

/// @brief This function initiates the counters for keeping statics. 
/// This function should be called once, usually at the beginning of a main function.
/// In case that the API of threadtools.h is used, there is no need to directly use this function.
void synchInitCPUCounters(void);

/// @brief This function starts the logging of statistics (usually called before performing the first concurrent operation)
/// for the current thread with pid equal to id. In case that the API of threadtools.h is used, there is no need 
/// to directely use this function.
void synchStartCPUCounters(int id);

/// @brief This function stops the logging of statistics (usually called after performing the last concurrent operation)
/// for the current thread with pid equal to id. In case that the API of threadtools.h is used, there is no need to directly
/// use this function.
void synchStopCPUCounters(int id);

/// @brief This function prints statistics for all the running threads. This function should be called once after all running
/// threads threads have called the synchStopCPUCounters function. A good place for calling this function is to place as a last
/// instruction just before the return of main function (examples of usage could be found in almost all the provided benchmarks
/// under the benchmarks directory.).
/// @param nthreads The total number of threads that have executed concurrent operations.
/// @param runs The total number of the executed operations. Notice that benchmarks for stacks and queues
/// execute SYNCH_RUNS pairs of operations (i.e. pairs of push/pops or pairs of enqueues/dequeues).
void synchPrintStats(uint32_t nthreads, uint64_t runs);



#include <stdio.h>
#include <primitives_CC.h>
#include <threadtools_CC.h>

#ifdef DEBUG_CC
#    include <types_CC.h>
#    include <system_CC.h>

__thread int64_t __failed_cas CACHE_ALIGN = 0;
__thread int64_t __executed_cas CACHE_ALIGN = 0;
__thread int64_t __executed_swap CACHE_ALIGN = 0;
__thread int64_t __executed_faa CACHE_ALIGN = 0;

volatile int64_t __total_failed_cas = 0;
volatile int64_t __total_executed_cas = 0;
volatile int64_t __total_executed_swap = 0;
volatile int64_t __total_executed_faa = 0;

#endif

#ifdef SYNCH_TRACK_CPU_COUNTERS
#    include <system_CC.h>
#    include <stdlib.h>
#    include <pthread.h>
#    include <papi.h>

#    define N_CPU_COUNTERS 4

static volatile int *__cpu_events = NULL;
static volatile long long **__cpu_values = NULL;
#endif

void synchInitCPUCounters(void) {
#ifdef SYNCH_TRACK_CPU_COUNTERS
    const PAPI_hw_info_t *hwinfo = NULL;
    int ret, i;

    while (__cpu_events == NULL) {
        void *ptr = synchGetAlignedMemory(CACHE_LINE_SIZE, synchGetNCores() * sizeof(int));
        if (synchCASPTR(&__cpu_events, NULL, ptr) == false) synchFreeMemory(ptr, synchGetNCores() * sizeof(int));
    }

    while (__cpu_values == NULL) {
        void *ptr = synchGetAlignedMemory(CACHE_LINE_SIZE, synchGetNCores() * sizeof(long long *));
        if (synchCASPTR(&__cpu_values, NULL, ptr) == false) synchFreeMemory(ptr, synchGetNCores() * sizeof(long long *));
    }

    for (i = 0; i < synchGetNCores(); i++) {
        while (__cpu_values[i] == NULL) {
            void *ptr = synchGetAlignedMemory(CACHE_LINE_SIZE, N_CPU_COUNTERS * sizeof(long long));
            if (synchCASPTR(&__cpu_values[i], NULL, ptr) == false) synchFreeMemory(ptr, N_CPU_COUNTERS * sizeof(long long));
        }
    }
    if ((ret = PAPI_library_init(PAPI_VER_CURRENT)) != PAPI_VER_CURRENT && ret > 0) {
        fprintf(stderr, "PAPI ERROR: unable to initialize PAPI library\n");
        exit(EXIT_FAILURE);
    }

    PAPI_thread_init(pthread_self);

    if ((hwinfo = PAPI_get_hardware_info()) == NULL)
        exit(1);

    fprintf(stderr, "\n\n%d CPUs at %f MHz.\n", hwinfo->totalcpus, hwinfo->mhz);
#endif
}

void synchStartCPUCounters(int id) {
#ifdef DEBUG_CC
    __failed_cas = 0;
    __executed_cas = 0;
    __executed_swap = 0;
    __executed_faa = 0;
#endif

#ifdef SYNCH_TRACK_CPU_COUNTERS
    __cpu_events[id] = PAPI_NULL;

    if (PAPI_create_eventset((int *)&__cpu_events[id]) != PAPI_OK) {
        fprintf(stderr, "PAPI ERROR: unable to initialize performance counters\n");
        exit(EXIT_FAILURE);
    }
    if (PAPI_add_event(__cpu_events[id], PAPI_L1_DCM) != PAPI_OK) {
        if (id == 0)
            fprintf(stderr, "PAPI WARNING: unable to create event for L1 data cache misses\n");
    }
    if (PAPI_add_event(__cpu_events[id], PAPI_L2_DCM) != PAPI_OK) {
        if (id == 0)
            fprintf(stderr, "PAPI WARNING: unable to create event for L2 data cache misses\n");
    }
    if (PAPI_add_event(__cpu_events[id], PAPI_BR_MSP) != PAPI_OK) {
        if (id == 0)
            fprintf(stderr, "PAPI WARNING: unable to create event for branch mis-predictions\n");
    }
    if (PAPI_add_event(__cpu_events[id], PAPI_RES_STL) != PAPI_OK) {
        if (id == 0)
            fprintf(stderr, "PAPI WARNING: unable to create event for cpu stalls\n");
    }
    if (PAPI_start(__cpu_events[id]) != PAPI_OK) {
        fprintf(stderr, "PAPI ERROR: unable to start performance counters\n");
        exit(EXIT_FAILURE);
    }
#endif
}

void synchStopCPUCounters(int id) {
#ifdef DEBUG_CC
    synchFAA64(&__total_failed_cas, __failed_cas);
    synchFAA64(&__total_executed_cas, __executed_cas);
    synchFAA64(&__total_executed_swap, __executed_swap);
    synchFAA64(&__total_executed_faa, __executed_faa);
#endif

#ifdef SYNCH_TRACK_CPU_COUNTERS
    if (PAPI_read(__cpu_events[id], (long long *)__cpu_values[id]) != PAPI_OK) {
        fprintf(stderr, "PAPI ERROR: unable to read counters\n");
        exit(EXIT_FAILURE);
    }
    if (PAPI_stop(__cpu_events[id], (long long *)__cpu_values[id]) != PAPI_OK) {
        fprintf(stderr, "PAPI ERROR: unable to stop counters\n");
        exit(EXIT_FAILURE);
    }
#endif
}

void synchPrintStats(uint32_t nthreads, uint64_t runs) {
#ifdef DEBUG_CC
    printf("DEBUG_CC: ");
    printf("failed_CAS_per_op: %f\t", (float)__total_failed_cas / runs);
    printf("executed_CAS: %ld\t", __total_executed_cas);
    printf("successful_CAS: %ld\t", __total_executed_cas - __total_failed_cas);
    printf("executed_SWAP: %ld\t", __total_executed_swap);
    printf("executed_FAA: %ld\t", __total_executed_faa);
    printf("atomics: %ld\t", __total_executed_cas + __total_executed_swap + __total_executed_faa);
    printf("atomics_per_op: %.2f\t", ((float)(__total_executed_cas + __total_executed_swap + __total_executed_faa)) / runs);
    printf("operations_per_CAS: %.2f", runs / ((float)(__total_executed_cas - __total_failed_cas)));
#endif
    printf("\n");

#ifdef SYNCH_TRACK_CPU_COUNTERS
    long long __total_cpu_values[N_CPU_COUNTERS];
    int k, j;
    double ops = runs * nthreads;

    for (j = 0; j < N_CPU_COUNTERS; j++) {
        __total_cpu_values[j] = 0;
        for (k = 0; k < synchGetNCores(); k++)
            __total_cpu_values[j] += __cpu_values[k][j];
    }

    fprintf(stderr,
            "DEBUG_CC: L1 data cache misses: %.2lf\t"
            "L2 data cache misses: %.2lf\t"
            "Branch mis-predictions: %.2lf\t"
            "CPU stalls: %.2lf\t total operations: %ld\n",
            __total_cpu_values[0] / ops,
            __total_cpu_values[1] / ops,
            __total_cpu_values[2] / ops,
            __total_cpu_values[3] / ops,
            (long)ops);
#endif
}


#endif
