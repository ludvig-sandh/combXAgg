#include "ebf_stack.hpp"

#include <algorithm>
#include <stdexcept>

#ifndef VERBOSE
#define VERBOSE if (0)
#endif

#ifndef COUTATOMIC
#define COUTATOMIC(coutstr) \
    do {                   \
    } while (0)
#endif

#include "elimination_backoff_stack.h"

namespace {

constexpr uint64_t kDefaultDelay = 15000;
constexpr size_t kThreadLocalPreallocPages = 1024;
thread_local bool thread_allocator_initialized = false;

uint64_t collision_array_size(int num_threads) {
    return static_cast<uint64_t>(std::max(1, (num_threads + 1) / 10));
}

void init_thread_allocator_once() {
    if (!thread_allocator_initialized) {
        scal::ThreadLocalAllocator::Get().Init(kThreadLocalPreallocPages, true);
        thread_allocator_initialized = true;
    }
}

}  // namespace

struct EbfStack::Impl {
    explicit Impl(int external_num_threads)
        : internal_num_threads(static_cast<uint64_t>(external_num_threads) + 1),
          stack(internal_num_threads,
                collision_array_size(external_num_threads),
                kDefaultDelay) {
    }

    int internal_tid(int tid) const {
        if (tid < 0 || static_cast<uint64_t>(tid) >= internal_num_threads - 1) {
            throw std::out_of_range("EbfStack thread id out of range");
        }
        return tid + 1;
    }

    uint64_t internal_num_threads;
    scal::EliminationBackoffStack<uint64_t> stack;
};

EbfStack::EbfStack(int num_threads) {
    if (num_threads <= 0) {
        throw std::invalid_argument("EbfStack requires at least one thread");
    }

    init_thread_allocator_once();
    impl_.reset(new Impl(num_threads));
}

EbfStack::~EbfStack() {
    impl_.reset();
    scal::ThreadLocalAllocator::GlobalDestroyAll();
    thread_allocator_initialized = false;
}

void EbfStack::register_thread(int tid) {
    (void)impl_->internal_tid(tid);
    init_thread_allocator_once();
}

bool EbfStack::push(int tid, uint64_t value) {
    return impl_->stack.push(value, impl_->internal_tid(tid));
}

bool EbfStack::pop(int tid, uint64_t* value) {
    if (value == nullptr) {
        throw std::invalid_argument("EbfStack::pop requires a non-null value");
    }
    return impl_->stack.pop(value, impl_->internal_tid(tid));
}
