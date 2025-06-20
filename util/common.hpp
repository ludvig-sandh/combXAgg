#pragma once

#pragma once

#include <atomic>
#include <stdexcept>
#include <thread>

#include "epoch.hpp"

static const int max_thread_count = 256; //FIXME: std::thread::hardware_concurrency();

class my_mutex {
   public:
    std::atomic<int> val = 0;
    void lock() {
        bool success = false;
        while (!success) {
            while (val.load() == 1);
            int expected = 0;
            success = val.compare_exchange_weak(expected, 1);
        }
    }
    bool try_lock() {
        int expected = 0;
        return val.compare_exchange_strong(expected, 1);
    }
    void unlock() { val.store(0); }
};

template <typename T>
struct atomic_128 {
    static_assert(sizeof(T) == 16, "T must be 128 bits");
    volatile T val;
    T compare_exchange(T &expected, T desired) {
        __int128_t *val_ptr = (__int128_t *)&val;
        __int128_t expected_val = *(__int128_t *)&expected;
        __int128_t desired_val = *(__int128_t *)&desired;
        __int128_t existed =
            __sync_val_compare_and_swap(val_ptr, expected_val, desired_val);
        return *(T *)&existed;
    }

    T load()  // not atomic
    {
        return val;
    }

    void store(T desired)  // not atomic
    {
        val = desired;
    }

    T load_atomic() {
        T read_1 = val;
        T read_2 = val;
        while (!(read_1 == read_2)) {
            read_1 = read_2;
            read_2 = T(val);
        }
        return read_1;
    }
};

template <typename T>
class MyStack {
    std::vector<T> data;
    int _size = 0;

   public:
    MyStack(int reserve_size = 64) {
        data = std::vector<T>(reserve_size);
        _size = 0;
    }
    void push(T val) {
        if (_size == data.size()) {
            data.resize(data.size() * 2);
        }
        data[_size++] = val;
    }
    T pop() { return data[--_size]; }
    void clear() { _size = 0; }
    bool empty() { return _size == 0; }
    int size() { return _size; }
};

class BaseMap {
   public:
    const int MAX_KEY = 2147483647;
    const int SENTINAL_VALUE = -1;  // NOT_FOUND
    const int NOT_FOUND = SENTINAL_VALUE;
    const int IS_RANGE_SUPPORTED = 0;

    BaseMap() {}
    // virtual bool insert(int key, int val) = 0;
    // virtual int remove(int key) = 0;
    // virtual int lookup(int key) = 0;
    // virtual bool search(int key) = 0;

    // /* Debug */
    // virtual int size() = 0;
    // virtual void print() = 0;
    // virtual long long sum_keys() = 0;
    // virtual long long sum_values() = 0;
};

class RangeQueryMap : public BaseMap {
   public:
    const int IS_RANGE_SUPPORTED = 1;

    RangeQueryMap() {}
    // virtual long long range_sum(int key_st, int key_ed) = 0;
};

struct RunResult {
    long long op_counts[2] = {0, 0};
    long long total_count = 0;
    long long random_work = 0;

    long long loop_count_1 = 0;  // wait for other work
    long long loop_count_2 = 0;  // traverse through the list

    long long root_access = 0;
};

template <typename T>
class Counter {
   public:
    virtual T fetch_add(T diff, int thread_id);
    virtual T load() const;
    virtual void store(T value,
                       std::memory_order order = std::memory_order_seq_cst);
    virtual bool compare_exchange(T &expected, T desired);
    long long max_access() const;
    long long root_access() const;
    void update_aux_data(int thread_id, RunResult &result) const;
};

template <typename T>
class EmptyCounter : public Counter<T> {
   public:
    EmptyCounter(int thread_count) {}
    T fetch_add(T diff, int thread_id) { return 0; }
    T load() const { return 0; }
    void store(T value, std::memory_order order = std::memory_order_seq_cst) {}
    bool compare_exchange(T &expected, T desired) { return false; }
    long long max_access() const { return 0; }
    long long root_access() const { return 0; }
    void update_aux_data(int thread_id, RunResult &result) const {}
};
