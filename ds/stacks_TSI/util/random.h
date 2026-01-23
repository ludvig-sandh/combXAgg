// Copyright (c) 2012-2013, the Scal Project Authors.  All rights reserved.
// Please see the AUTHORS file for details.  Use of this source code is governed
// by a BSD license that can be found in the LICENSE file.

#ifndef SCAL_UTIL_RANDOM_H_
#define SCAL_UTIL_RANDOM_H_

#include <inttypes.h>
#include <stdio.h>

#include <chrono>
#include <random>

namespace {

// Used for pseudorand
const uint32_t kA = 16807;
const uint32_t kM = 2147483647;
const uint32_t kQ = 127773;
const uint32_t kR = 2836;

}  // namespace

#include "threadlocals.h"
namespace scal {

namespace detail {

inline uint64_t rdtsc() {
    unsigned int hi, lo;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)lo) | (((uint64_t)hi) << 32);
}

}  // namespace detail

const uint32_t kRandMax = 2147483647;

uint64_t pseudorand();
uint64_t pseudorandrange(uint32_t min, uint32_t max);
void srand(uint32_t seed);

inline uint64_t hwrand() { return (detail::rdtsc() >> 6); }

// Fisher Yates shuffle.
// Do not use the simple linear contruential PRNG.
template <typename T>
void shuffle(T* items, size_t len, uint64_t seed = 0) {
    if (seed == 0) {
        std::random_device rd;
        seed = rd();
    }
    std::mt19937_64 rng(seed);
    for (size_t i = (len - 1); i > 0; --i) {
        std::uniform_int_distribution<size_t> dist(0, i);
        size_t swap_idx = dist(rng);
        T tmp = items[swap_idx];
        items[swap_idx] = items[i];
        items[i] = tmp;
    }
}

uint64_t pseudorand() {
    int32_t seed = scal::ThreadContext::get().random_seed();
    uint32_t hi = seed / kQ;
    uint32_t lo = seed % kQ;
    seed = kA * lo - kR * hi;
    if (seed < 0) {
        seed += kM;
    }
    scal::ThreadContext::get().set_random_seed(seed);
    return seed;
}

uint64_t pseudorandrange(uint32_t min, uint32_t max) {
    uint32_t range = max - min;
    return (static_cast<double>(pseudorand()) /
            (static_cast<double>(kM) + 1.0)) *
               static_cast<double>(range) +
           min;
}

void srand(uint32_t seed) { ThreadContext::get().set_random_seed(seed); }

}  // namespace scal

#include "threadlocals.h"
#endif  // SCAL_UTIL_RANDOM_H_
