// Copyright (c) 2012-2013, the Scal Project Authors.  All rights reserved.
// Please see the AUTHORS file for details.  Use of this source code is governed
// by a BSD license that can be found in the LICENSE file.

#ifndef SCAL_UTIL_THREADLOCALS_H_
#define SCAL_UTIL_THREADLOCALS_H_

#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include <new>

#include "platform.h"

namespace scal {
uint64_t hwrand();
class ThreadContext {
   public:
    static ThreadContext &get();
    static void prepare(uint64_t num_threads);
    static void assign_context();

    static constexpr uint64_t get_max_threads() { return kMaxThreads; }

    inline uint64_t thread_id() { return thread_id_; }

    inline void new_random_seed();

    inline void set_random_seed(uint32_t seed) { random_seed_ = seed; }

    inline int32_t random_seed() { return random_seed_; }

    inline void set_data(void *data) { data_ = data; }

    inline void *get_data() { return data_; }

   private:
    static constexpr uint64_t kMaxThreads = 1024;
    static uint64_t global_thread_id_cnt;
    static ThreadContext *contexts[kMaxThreads];
    static pthread_key_t threadcontext_key;

    ThreadContext() {}
    ThreadContext(ThreadContext const &cpy);
    void operator=(ThreadContext const &rhs);

    uint64_t thread_id_;
    int32_t random_seed_;
    void *data_;
};

uint64_t ThreadContext::global_thread_id_cnt = 0;
pthread_key_t ThreadContext::threadcontext_key;
ThreadContext *ThreadContext::contexts[kMaxThreads];
#include "random.h"
inline void ThreadContext::new_random_seed() {
    random_seed_ = 
        static_cast<uint32_t>(hwrand()) + (thread_id() + 1 * 100);
}

ThreadContext &ThreadContext::get() {
    if (pthread_getspecific(threadcontext_key) == NULL) {
        assign_context();
    }
    ThreadContext *context =
        static_cast<ThreadContext *>(pthread_getspecific(threadcontext_key));
    return *context;
}

void ThreadContext::assign_context() {
    uint64_t thread_id = __sync_fetch_and_add(&global_thread_id_cnt, 1);
    if (pthread_setspecific(threadcontext_key, contexts[thread_id])) {
        fprintf(stderr, "%s: pthread_setspecific failed\n", __func__);
        exit(EXIT_FAILURE);
    }
}

void ThreadContext::prepare(uint64_t num_threads) {
    pthread_key_create(&threadcontext_key, NULL);
    size_t size =
        (sizeof(ThreadContext) / scal::kPageSize + 1) * scal::kPageSize;
    void *mem;
    for (uint64_t i = 0; i < num_threads; i++) {
        if (posix_memalign(&mem, scal::kPageSize, size)) {
            fprintf(stderr, "%s: posix_memalign failed\n", __func__);
            exit(EXIT_FAILURE);
        }
        ThreadContext *context = new (mem) ThreadContext();
        context->thread_id_ = i;
        context->new_random_seed();
        contexts[i] = context;
        context->data_ = NULL;
    }
};

}  // namespace scal

#endif  // SCAL_UTIL_THREADLOCALS_H_