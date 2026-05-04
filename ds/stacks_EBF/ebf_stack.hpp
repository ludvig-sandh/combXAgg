#ifndef EBF_STACK_WRAPPER_HPP
#define EBF_STACK_WRAPPER_HPP

#include <cstdint>
#include <memory>

class EbfStack {
   public:
    explicit EbfStack(int num_threads);
    ~EbfStack();

    EbfStack(const EbfStack&) = delete;
    EbfStack& operator=(const EbfStack&) = delete;

    void register_thread(int tid);
    bool push(int tid, uint64_t value);
    bool pop(int tid, uint64_t* value);

   private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

#endif
