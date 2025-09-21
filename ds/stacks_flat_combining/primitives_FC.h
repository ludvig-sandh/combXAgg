/// @file primitives.h
/// @brief This file exposes a simple API for basic atomic operations, basic operations for memory management.
/// Moreover, this file provides functionality for finding out the the vendor of the processor and some 
/// basic functionality for measuring time.
#ifndef _PRIMITIVES_H_
#define _PRIMITIVES_H_

#include <stdint.h>
#include <stdbool.h>
#include <config_FC.h>
#include <system_FC.h>
// #include <stats.h>
#include <stddef.h>
#include <malloc.h> // for memalign

/// @brief The vendor of the processor is unknown.
#define UNKNOWN_MACHINE             0x0
/// @brief The vendor is an AMD x86 processor.
#define AMD_X86_MACHINE             0x1
/// @brief The vendor is an Intel X86 processor.
#define INTEL_X86_MACHINE           0x2
/// @brief This is a generic X86 processor.
#define X86_GENERIC_MACHINE         0x3
/// @brief This is a generic ARM processor.
#define ARM_GENERIC_MACHINE         0x4
/// @brief This is a generic RISC-V processor.
#define RISCV_GENERIC_MACHINE       0x5
/// @brief This is return whenever the system is not initialized.
#define UNINITIALIZED_MACHINE_MODEL 0xFFFFFFFF

#if defined(__GNUC__) && (__GNUC__ * 10000 + __GNUC_MINOR__ * 100) >= 40100
#    define __CAS128(A, B0, B1, C0, C1) _CAS128(A, B0, B1, C0, C1)
#    define __CASPTR(A, B, C)           __sync_bool_compare_and_swap((long *)A, (long)B, (long)C)
#    define __CAS64(A, B, C)            __sync_bool_compare_and_swap(A, B, C)
#    define __CAS32(A, B, C)            __sync_bool_compare_and_swap(A, B, C)
#    define __SWAP(A, B)                __sync_lock_test_and_set((long *)A, (long)B)
#    define __FAA64(A, B)               __sync_fetch_and_add(A, B)
#    define __FAA32(A, B)               __sync_fetch_and_add(A, B)
#    define __BitTAS64(A, B)            __sync_fetch_and_or(A, (1ULL << (B)))
#    define synchReadPrefetch(A)        __builtin_prefetch((const void *)A, 0, 3);
#    define synchStorePrefetch(A)       __builtin_prefetch((const void *)A, 1, 3);
#    define synchBitSearchFirst(A)      __builtin_ctzll(A)
#    define synchNonZeroBits(A)         __builtin_popcountll(A)
#    define synchLikely(A)              __builtin_expect(!!(A), 1)
#    define synchUnlikely(A)            __builtin_expect(!!(A), 0)
#    define UNUSED_ARG                  __attribute__((unused))
#    if defined(__amd64__) || defined(__x86_64__)
#        define synchLoadFence()  asm volatile("lfence" ::: "memory")
#        define synchStoreFence() asm volatile("sfence" ::: "memory")
#        define synchFullFence()  asm volatile("mfence" ::: "memory")
#        define synchNonTSOFence()
#    else
#        define synchLoadFence()   __sync_synchronize()
#        define synchStoreFence()  __sync_synchronize()
#        define synchFullFence()   __sync_synchronize()
#        define synchNonTSOFence() __sync_synchronize()
#    endif
#elif defined(__GNUC__) && (defined(__amd64__) || defined(__x86_64__))
#    warning You may lose performance!
#    warning A newer version of GCC compiler is recommended!
#    define synchLoadFence()      asm volatile("lfence" ::: "memory")
#    define synchStoreFence()     asm volatile("sfence" ::: "memory")
#    define synchFullFence()      asm volatile("mfence" ::: "memory")
#    define synchReadPrefetch(A)  asm volatile("prefetchnta %0" ::"m"(*((const int *)A)))
#    define synchStorePrefetch(A) asm volatile("prefetchnta %0" ::"m"(*((const int *)A)))
#    define synchNonTSOFence()
#    define synchLikely(A)   (A)
#    define synchUnlikely(A) (A)
#    define UNUSED_ARG       __attribute__((unused))
//   in this case where gcc is too old, implement atomic primitives in primitives.c
#    define __OLD_GCC_X86__
inline int synchBitSearchFirst(uint64_t B);
inline uint64_t synchNonZeroBits(uint64_t v);
#else
#    error Current machine architecture and compiler are not supported yet!
#endif

#if defined(__GNUC__) && (defined(__amd64__) || defined(__x86_64__))
/// @brief This macro is designed to emit a pause instruction specifically for Intel X86 processors.
/// The inclusion of a pause instruction in spinning loops can significantly improve performance
/// on Intel X86 machines by reducing the execution resource requirements and improving the performance
/// in many SMT scenarios. It effectively hints to the processor that it is in a spin-wait loop,
/// allowing the CPU to handle the other thread more efficiently or reduce power consumption.
///
/// Note: This optimization is particularly beneficial on Intel X86 architectures and does not yield
/// the same performance benefits on AMD processors.
#    define synchPause()                                                                                                                                                                               \
        {                                                                                                                                                                                              \
            int __i;                                                                                                                                                                                   \
            if (synchGetMachineModel() != INTEL_X86_MACHINE)                                                                                                                                           \
                return;                                                                                                                                                                                \
            for (__i = 0; __i < 16; __i++) {                                                                                                                                                           \
                asm volatile("pause");                                                                                                                                                                 \
                asm volatile("pause");                                                                                                                                                                 \
                asm volatile("pause");                                                                                                                                                                 \
                asm volatile("pause");                                                                                                                                                                 \
            }                                                                                                                                                                                          \
        }
#else
/// @brief This macro emits a NO-OP instruction.
#    define synchPause()
#endif

/// @brief This function allocates a memory area of size bytes.
/// In case that SYNCH_NUMA_SUPPORT is defined in libconcurrent/config.h, the returned memory is allocated on the local NUMA node.
///
/// @param size The size of the memory area.
/// @return In case of error, NULL is returned. In case of success a pointer to the allocated memory area is returned.
inline void *synchGetMemory(size_t size);

/// @brief This function allocates a memory area of size bytes. The returned address is aligned to an offset equal to align bytes.
/// In case that SYNCH_NUMA_SUPPORT is defined in libconcurrent/config.h, the returned memory is allocated on the local NUMA node.
///
/// @param align The alignment size.
/// @param size The size of the memory area.
/// @return In case of error, NULL is returned. In case of success a pointer to the allocated memory area is returned.
inline void *synchGetAlignedMemory(size_t align, size_t size);
// {
//         void *p;


// // FIXME: UNCOMMENT NUMA stuff
// // #ifdef SYNCH_NUMA_SUPPORT
// //     p = numa_alloc_local(size + align);
// //     long plong = (long)p;
// //     plong += align;
// //     plong &= ~(align - 1);
// //     p = (void *)plong;
// // #else
//     p = (void *)memalign(align, size);
// // #endif

//     if (p == NULL) {
//         perror("memory allocation fail");
//         exit(1);
//     } else
//         return p;
// }

/// @brief This function frees memory allocated with either getMemory() or synchGetAlignedMemory() functions.
///
/// @param ptr A pointer to the memory area to be freed.
/// @param size The size of the memory area to be freed.
inline void synchFreeMemory(void *ptr, size_t size);

/// @brief This function returns the current system's time in milliseconds.
///
/// @return System's time in milliseconds.
inline int64_t synchGetTimeMillis(void);

/// @brief This function returns the vendor of the processor that it runs on.
/// The current version of the Synch framework returns any of the following codes:
/// - AMD_X86_MACHINE
/// - INTEL_X86_MACHINE
/// - X86_GENERIC_MACHINE
/// - ARM_GENERIC_MACHINE
/// - RISCV_GENERIC_MACHINE
/// - UNKNOWN_MACHINE
///
/// @return It returns a code for the vendor of the processor that it runs on.
inline uint64_t synchGetMachineModel(void);

/// A wrapper for the _CAS128 function. See more on _CAS128().
#define synchCAS128(A, B0, B1, C0, C1) _CAS128((uint64_t *)(A), (uint64_t)(B0), (uint64_t)(B1), (uint64_t)(C0), (uint64_t)(C1))
/// @brief This function is executed atomically. It reads the 128-bit value stored at a memory location pointed by A. 
/// Let A0 be the less significant 64-bits of this memory location and let A1 the most significant.
/// This function computes '(A0 == B0 & A1 == B1) ? <C0,C1> : <A0,A1>' and stores the result at location pointed by A.
/// The function returns in case that (A0 == B0 & A1 == B1) is true. Otherwise, it returns false.
///
/// @param A A pointer to 128-bit value stored to memory.
/// @param B0 The less significant 64-bits of the old value.
/// @param B1 The most significant 64-bits of the old value.
/// @param C0 The less significant 64-bits of the new value.
/// @param C1 The most significant 64-bits of the new value.
/// @return This function computes '(A0 == B0 & A1 == B1) ? <C0,C1> : <A0,A1>' and stores the result at location pointed by A.
/// The function returns in case that (A0 == B0 & A1 == B1) is true. Otherwise, it returns false.
inline bool _CAS128(uint64_t *A, uint64_t B0, uint64_t B1, uint64_t C0, uint64_t C1);

/// A wrapper for the _CASPTR function. See more on _CASPTR().
#define synchCASPTR(A, B, C) _CASPTR((void *)(A), (void *)(B), (void *)(C))
/// @brief This function is executed atomically. It reads the pointer stored at a memory location pointed by A. 
/// Let OLD be the pointer of the memory location pointed by A. This function computes '(OLD == B) ? C : OLD' and stores the result
/// at location pointed by A. The function returns in case that (OLD == B) is true. Otherwise, it returns false.
///
/// @param A A pointer to memory location that stores a memory pointer.
/// @param B The old value.
/// @param C The new value.
/// @return The function returns in case that (OLD == B) is true. Otherwise, it returns false.
inline bool _CASPTR(void *A, void *B, void *C);

/// A wrapper for the _CAS64 function. See more on _CAS64().
#define synchCAS64(A, B, C) _CAS64((uint64_t *)(A), (uint64_t)(B), (uint64_t)(C))
/// @brief This function is executed atomically. It reads the 64-bit value stored at a memory location pointed by A. 
/// Let OLD be the value of the memory location pointed by A. This function computes '(OLD == B) ? C : OLD' and stores the result
/// at location pointed by A. The function returns in case that (OLD == B) is true. Otherwise, it returns false.
///
/// @param A A pointer to 64-bit value stored to memory.
/// @param B The old value (64-bits).
/// @param C The new value (64-bits).
/// @return The function returns in case that (OLD == B) is true. Otherwise, it returns false.
inline bool _CAS64(uint64_t *A, uint64_t B, uint64_t C);

/// A wrapper for the _CAS32 function. See more on _CAS32().
#define synchCAS32(A, B, C) _CAS32((uint32_t *)(A), (uint32_t)(B), (uint32_t)(C))
/// @brief This function is executed atomically. It reads the 32-bit value stored at a memory location pointed by A. 
/// Let OLD be the value of the memory location pointed by A. This function computes '(OLD == B) ? C : OLD' and stores the result
/// at location pointed by A. The function returns in case that (OLD == B) is true. Otherwise, it returns false.
///
/// @param A A pointer to 32-bit value stored to memory.
/// @param B The old value (32-bits).
/// @param C The new value (32-bits).
/// @return The function returns in case that (OLD == B) is true. Otherwise, it returns false.
inline bool _CAS32(uint32_t *A, uint32_t B, uint32_t C);

/// A wrapper for the _SWAP function. See more on _SWAP().
#define synchSWAP(A, B) _SWAP((void *)(A), (void *)(B))
/// @brief This function is executed atomically. It performs an atomic exchange operation on the memory location pointed by A,
/// setting B as the new value. It returns the old value of the memory location pointed by A just before the operation.
///
/// @param A A pointer to memory location that stores a memory pointer.
/// @param B The new value to be stored in the memory location pointed by A.
/// @return It returns the old value of the memory location pointed by A just before the operation. 
inline void *_SWAP(void *A, void *B);

/// A wrapper for the _FAA32 function. See more on _FAA32().
#define synchFAA32(A, B) _FAA32((volatile int32_t *)(A), (int32_t)(B))
/// @brief This function is executed atomically. It performs an atomic addition of value B to the value pointed by A.
/// It returns the old value of the memory location pointed by A just before the operation.
///
/// @param A A pointer to memory location that stores a 32-bit integer.
/// @param B A 32-bit integer to be added to the value pointed by A.
/// @return It returns the old 32-bit value of the memory location pointed by A just before the operation.
inline int32_t _FAA32(volatile int32_t *A, int32_t B);

/// A wrapper for the _FAA64 function. See more on _FAA64().
#define synchFAA64(A, B) _FAA64((volatile int64_t *)(A), (int64_t)(B))
/// @brief This function is executed atomically. It performs an atomic addition of value B to the value pointed by A.
/// It returns the old value of the memory location pointed by A just before the operation.
///
/// @param A A pointer to memory location that stores a 64-bit integer.
/// @param B A 64-bit integer to be added to the value pointed by A.
/// @return It returns the old 64-bit value of the memory location pointed by A just before the operation.
inline int64_t _FAA64(volatile int64_t *A, int64_t B);

/// A wrapper for the _BitTAS64 function. See more on _BitTAS64().
#define synchBitTAS64(A, B) _BitTAS64((volatile uint64_t *)(A), (unsigned char)(B))
/// @brief This function is executed atomically. It performs an atomic Test&Set on the B-th bit of the value pointed by A.
///
/// @param A A pointer to memory location that stores a 64-bit value.
/// @param B The B-th bit of the value stored in the memory location pointed by A.
/// @return The result of Test&Set on the B-th bit of the value pointed by A.
inline uint64_t _BitTAS64(volatile uint64_t *A, unsigned char B);



// #include <malloc.h>
// #include <primitives.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


#ifdef SYNCH_NUMA_SUPPORT
#include <numa.h>
#endif

#define MAX_VENDOR_STR_SIZE 64

static __thread uint32_t __machine_model = UNINITIALIZED_MACHINE_MODEL;

#ifdef DEBUG_FC_STACK
extern __thread int64_t __failed_cas;
extern __thread int64_t __executed_cas;
// extern __thread int64_t __executed_swap;
__thread int64_t __executed_swap CACHE_ALIGN = 0;

// extern __thread int64_t __executed_faa;
__thread int64_t __executed_faa CACHE_ALIGN = 0;
#endif

#ifdef __OLD_GCC_X86__
inline bool __CASPTR(void *A, void *B, void *C) {
    uint64_t prev;
    uint64_t *p = (uint64_t *)A;

    asm volatile(
        "lock;"
        "cmpxchgq %1,%2"
        : "=a"(prev)
        : "r"((uint64_t)C), "m"(*p), "0"((uint64_t)B)
        : "memory");
    return (prev == (uint64_t)B);
}

inline bool __CAS64(volatile uint64_t *A, uint64_t B, uint64_t C) {
    uint64_t prev;
    uint64_t *p = (uint64_t *)A;

    asm volatile(
        "lock;"
        "cmpxchgq %1,%2"
        : "=a"(prev)
        : "r"(C), "m"(*p), "0"(B)
        : "memory");
    return (prev == B);
}

inline bool __CAS32(uint32_t *A, uint32_t B, uint32_t C) {
    uint32_t prev;
    uint32_t *p = (uint32_t *)A;

    asm volatile(
        "lock;"
        "cmpxchgl %1,%2"
        : "=a"(prev)
        : "r"(C), "m"(*p), "0"(B)
        : "memory");
    return (prev == B);
}

inline void *__SWAP(void *A, void *B) {
    int64_t *p = (int64_t *)A;

    asm volatile(
        "lock;"
        "xchgq %0, %1"
        : "=r"(B), "=m"(*p)
        : "0"(B), "m"(*p)
        : "memory");
    return B;
}

inline int64_t __FAA64(volatile int64_t *A, int64_t B) {
    asm volatile(
        "lock;"
        "xaddq %0, %1"
        : "=r"(B), "=m"(*A)
        : "0"(B), "m"(*A)
        : "memory");
    return B;
}

inline int32_t __FAA32(volatile int32_t *A, int32_t B) {
    asm volatile(
        "lock;"
        "xaddl %0, %1"
        : "=r"(B), "=m"(*A)
        : "0"(B), "m"(*A)
        : "memory");
    return B;
}

inline uint64_t __BitTAS64(volatile uint64_t *A, unsigned char B) {
    int64_t *p = (int64_t *)A;
    int64_t bit = B;
    asm volatile(
        "lock;"
        "btsq %0, %1"
        : "=r"(bit), "=m"(*p)
        : "0"(bit), "m"(*p)
        : "memory");

    return bit;
}

inline int synchBitSearchFirst(uint64_t B) {
    uint64_t A;

    asm("bsfq %0, %1;" : "=d"(A) : "d"(B));

    return (int)A;
}

inline uint64_t synchNonZeroBits(uint64_t v) {
    uint64_t c;

    for (c = 0; v; v >>= 1) c += v & 1;

    return c;
}
#endif

inline bool _CAS128(uint64_t *A, uint64_t B0, uint64_t B1, uint64_t C0,
                    uint64_t C1) {
    bool res;

#if defined(__OLD_GCC_X86__) || defined(__amd64__) || defined(__x86_64__)
    uint64_t dummy;

    asm volatile(
        "lock;"
        "cmpxchg16b %2; setz %1"
        : "=d"(dummy), "=a"(res), "+m"(*A)
        : "b"(C0), "c"(C1), "a"(B0), "d"(B1));
#else
    __uint128_t old_value = (__uint128_t)(B0) | (((__uint128_t)(B1)) << 64ULL);
    __uint128_t new_value = (__uint128_t)(C0) | (((__uint128_t)(C1)) << 64ULL);
    res = __atomic_compare_exchange_16(A, &old_value, new_value, 0,
                                       __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#endif

#ifdef DEBUG_FC_STACK
    __executed_cas++;
    __failed_cas += 1 - res;
#endif

    return res;
}

inline void *synchGetMemory(size_t size) {
    void *p;

#ifdef SYNCH_NUMA_SUPPORT
    p = numa_alloc_local(size);
#else
    p = malloc(size);
#endif
    if (p == NULL) {
        perror("memory allocation fail");
        exit(EXIT_FAILURE);
    } else
        return p;
}

inline void *synchGetAlignedMemory(size_t align, size_t size) {
    void *p;

#ifdef SYNCH_NUMA_SUPPORT
    p = numa_alloc_local(size + align);
    long plong = (long)p;
    plong += align;
    plong &= ~(align - 1);
    p = (void *)plong;
#else
    p = (void *)memalign(align, size);
#endif

    if (p == NULL) {
        perror("memory allocation fail");
        exit(EXIT_FAILURE);
    } else
        return p;
}

inline void synchFreeMemory(void *ptr, size_t size) {
#ifdef SYNCH_NUMA_SUPPORT
    numa_free(ptr, size);
#else
    free(ptr);
#endif
}

inline int64_t synchGetTimeMillis(void) {
    struct timespec tm;

    if (clock_gettime(CLOCK_MONOTONIC, &tm) == -1) {
        perror("clock_gettime");
        return 0;
    } else
        return tm.tv_sec * 1000LL + tm.tv_nsec / 1000000LL;
}

inline uint64_t synchGetMachineModel(void) {
    if (__machine_model != UNINITIALIZED_MACHINE_MODEL) return __machine_model;

#if defined(__amd64__) || defined(__x86_64__)
    char cpu_model[MAX_VENDOR_STR_SIZE] = {'\0'};

    asm volatile(
        "movl $0, %%eax\n"
        "cpuid\n"
        "movl %%ebx, %0\n"
        "movl %%edx, %1\n"
        "movl %%ecx, %2\n"
        : "=m"(cpu_model[0]), "=m"(cpu_model[4]), "=m"(cpu_model[8])::"%eax",
          "%ebx", "%edx", "%ecx", "memory");
#ifdef DEBUG_FC_STACK
    // fprintf(stderr, "DEBUG_FC_STACK: Machine model: %s\n", cpu_model);
#endif

    if (strcmp(cpu_model, "AuthenticAMD") == 0)
        __machine_model = AMD_X86_MACHINE;
    else if (strcmp(cpu_model, "GenuineIntel") == 0)
        __machine_model = INTEL_X86_MACHINE;
    else
        __machine_model = X86_GENERIC_MACHINE;
#elif defined(__aarch64__)
    __machine_model = ARM_GENERIC_MACHINE;
#elif defined(__riscv__) || defined(__riscv)
    __machine_model = RISCV_GENERIC_MACHINE;
#else
    __machine_model = UNKNOWN_MACHINE;
#endif

    return __machine_model;
}

inline bool _CASPTR(void *A, void *B, void *C) {
#ifdef DEBUG_FC_STACK
    int res;

    res = __CASPTR(A, B, C);
    __executed_cas++;
    __failed_cas += 1 - res;

    return res;
#else
    return __CASPTR(A, B, C);
#endif
}

inline bool _CAS64(uint64_t *A, uint64_t B, uint64_t C) {
#ifdef DEBUG_FC_STACK
    int res;

    res = __CAS64(A, B, C);
    __executed_cas++;
    __failed_cas += 1 - res;

    return res;
#else
    return __CAS64(A, B, C);
#endif
}

inline bool _CAS32(uint32_t *A, uint32_t B, uint32_t C) {
#ifdef DEBUG_FC_STACK
    int res;

    res = __CAS32(A, B, C);
    __executed_cas++;
    __failed_cas += 1 - res;

    return res;
#else
    return __CAS32(A, B, C);
#endif
}

inline void *_SWAP(void *A, void *B) {
#if defined(SYNCH_EMULATE_SWAP)
#warning synchSWAP instructions are simulated!
    void *old_val;
    void *new_val;

    while (true) {
        old_val = (void *)*((volatile long *)A);
        new_val = B;
        if (((void *)*((volatile long *)A)) == old_val &&
            synchCASPTR(A, old_val, new_val) == true)
            break;
    }
#ifdef DEBUG_FC_STACK
    __executed_swap++;
#endif
    return old_val;
#else
#ifdef DEBUG_FC_STACK
    __executed_swap++;
    return (void *)__SWAP(A, B);
#else
    return (void *)__SWAP(A, B);
#endif
#endif
}

inline int32_t _FAA32(volatile int32_t *A, int32_t B) {
#if defined(SYNCH_EMULATE_FAA)
#warning Fetch&Add instructions are simulated!

    int32_t old_val;
    int32_t new_val;

    while (true) {
        old_val = *((int32_t *volatile)A);
        new_val = old_val + B;
        if (*A == old_val && synchCAS32(A, old_val, new_val) == true) break;
    }
#ifdef DEBUG_FC_STACK
    __executed_faa++;
#endif
    return old_val;
#else
#ifdef DEBUG_FC_STACK
    __executed_faa++;
    return __FAA32(A, B);
#else
    return __FAA32(A, B);
#endif
#endif
}

inline int64_t _FAA64(volatile int64_t *A, int64_t B) {
#if defined(SYNCH_EMULATE_FAA)
#warning Fetch&Add instructions are simulated!

    int64_t old_val;
    int64_t new_val;

    while (true) {
        old_val = *((int64_t *volatile)A);
        new_val = old_val + B;
        if (*A == old_val && synchCAS64(A, old_val, new_val) == true) break;
    }
#ifdef DEBUG_FC_STACK
    __executed_faa++;
#endif
    return old_val;
#else
#ifdef DEBUG_FC_STACK
    __executed_faa++;
    return __FAA64(A, B);
#else
    return __FAA64(A, B);
#endif
#endif
}

inline uint64_t _BitTAS64(volatile uint64_t *A, unsigned char B) {
    return __BitTAS64(A, B);
}

#endif
