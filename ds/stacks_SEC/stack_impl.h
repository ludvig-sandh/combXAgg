/**
 * @file stack_impl.h
 * @author Nikosmet Ajay
 * @brief A high-performance concurrent, lock-free stack implementation using
 * batch combining and elimination.
 * @version 0.1
 * @date 2025-05-09
 *
 * @copyright Copyright (c) 2025
 *
 * @details
 * This stack implementation is designed for high throughput in multi-threaded
 * environments by avoiding direct, frequent contention on the global stack top.
 *
 * Key Concepts:
 * 1.  **Aggregators:** Threads are partitioned into groups, each assigned to an
 * `Aggregator`. This reduces global contention.
 * 2.  **Batching:** Each `Aggregator` has a `Batch` object. Threads add
 * their operations (push or pop) to this current batch.
 * 3.  **Freezer Election:** The first thread to arrive at a new batch tries to
 * become the 'freezer'.
 * 4.  **Freezing:** The freezer 'freezes' the batch by taking a snapshot of the
 * operation counters (`pushCounter`, `popCounter`) and adding a new,
 * empty batch. This signals all threads in the current batch to proceed.
 * 5.  **Elimination:** After freezing, threads check if they can get
 * eliminated. Pushes and pops within the batch are paired up and 'eliminate'
 * each other via the `eliminationArray`.
 * 6.  **Combining:**
 * After elimination occurs the remaining threads apply their operation to the
 * shared stack, by appointing one combiner thread which is the thread with the
 * smallest "Index". This thread is the leader and applies all the operations in
 * the shared stack at once. All the other threads wait for their operations to
 * be applied.
 */

#ifndef STACK_IMPL_H
#define STACK_IMPL_H

/// @brief Max threads per aggregator. Calculated in constructor based on the
/// total thread count and NUMBER_AGGREGATORS.
int MAX_AGGREGATOR_THREADS;

/// @brief The total number of aggregators. Threads are partitioned across
/// these.

#if defined EXP_AGG1
#define NUMBER_AGGREGATORS 1
#elif defined EXP_AGG2
#define NUMBER_AGGREGATORS 2
#elif defined EXP_AGG3
#define NUMBER_AGGREGATORS 3
#elif defined EXP_AGG4
#define NUMBER_AGGREGATORS 4
#elif defined EXP_AGG5
#define NUMBER_AGGREGATORS 5
#else
#define NUMBER_AGGREGATORS 2
#endif

/// @brief Flag to enable retiring the entire Batch object.
#define BATCH_RETIRE

/// @brief Flag to enable retiring individual nodes.
#define NODE_RETIRE

/// @brief Macro to map a thread ID (tId) to its designated aggregator instance.
#define CHOOSE_AGGREGATOR(tId) (aggregator[(tId) / MAX_AGGREGATOR_THREADS])

#include "record_manager.h"  // For memory reclamation

/// @brief Thread-local flag to track if `initThread` has been called.
static __thread bool init = false;


/**
 * @brief Represents a single node in the stack.
 *
 * @tparam K Key type.
 * @tparam V Value type stored in the node.
 */
template <typename K, typename V>
class alignas(BYTES_IN_CACHE_LINE) node_t {
   public:
    K key;                            ///< Node key.
    V val;                            ///< Node value.
    std::atomic<node_t<K, V>*> next;  ///< Atomic pointer to the next node.

    node_t(K key, V val) {
        key = key;
        val = val;
        next = nullptr;
    }

    node_t() {
        key = 0;
        val = 0;
        next = nullptr;
    }
};
#define nodeptr node_t<K, V>*

/**
 * @brief Represents a collection of operations (a "batch").
 *
 * This is the central coordination structure. Threads from the same
 * aggregator group compete to add their operations to this batch.
 *
 * @tparam K Key type.
 * @tparam V Value type.
 */
template <typename K, typename V>
struct alignas(PREFETCH_SIZE_BYTES) Batch {
    /// @brief Array for push/pop elimination.
    /// Pushes store their node at `eliminationArray[pushIndex]`.
    /// Pops (with `popIndex < finalPushCount`) read from
    /// `eliminationArray[popIndex]`.
    std::atomic<nodeptr>* eliminationArray;
    PAD;
    /// @brief Atomic counter for pushes claiming a slot in this batch.
    std::atomic<int> pushCounter;
    PAD;
    /// @brief Atomic counter for pops claiming a slot in this batch.
    std::atomic<int> popCounter;
    PAD;
    /// @brief Snapshot of `pushCounter` taken by the freezer thread.
    /// This value is immutable once set and defines the batch's work.
    std::atomic<int> finalPushCount;
    PAD;
    /// @brief Snapshot of `popCounter` taken by the freezer thread.
    std::atomic<int> finalPopCount;
    PAD;
    /// @brief Flag set by the combiner thread after it has applied
    /// the batch's remaining operations (push or pop) to the `main_top`.
    /// Follower threads spin on this.
    std::atomic<bool> isBatchApplied;
    PAD;
    /// @brief Atomic flag used to elect a single 'freezer' thread for this
    /// batch.
    std::atomic_flag hasLeader;
    PAD;
    /// @brief Used by the pop-combiner to store the head of the sub-stack
    /// it popped from `main_top`. Follower pops read from this list.
    std::atomic<nodeptr> subStackTop;
    PAD;
    ~Batch() {
        if (eliminationArray) free(eliminationArray);
    }
};

/**
 * @brief Manages a partition of threads and their current `Batch`.
 *
 * @tparam K Key type.
 * @tparam V Value type.
 */
template <typename K, typename V>
struct alignas(PREFETCH_SIZE_BYTES) Aggregator {
    PAD;
    /// @brief Atomic pointer to the *current* active batch.
    /// When the freezer thread 'freezes' a batch, it atomically updates this
    /// pointer to a new batch. This change signals waiting threads.
    std::atomic<struct Batch<K, V>*> batch;
    PAD;
};

/**
 * @brief The main concurrent stack class.
 *
 * @tparam K Key type.
 * @tparam V Value type.
 * @tparam RecMgr The memory reclamation manager class.
 */
template <typename K, typename V, class RecMgr>
class alignas(BYTES_IN_CACHE_LINE) Stack {
   private:
    PAD;
    /**
     * @brief Thread-local pointer to a pre-allocated "next" batch.
     * When a thread becomes a freezer, it uses its `newBatchPtr` to
     * instantly install the next batch, avoiding allocation in the
     * critical path. It then allocates a new one for itself for the
     * *next* time it becomes a freezer.
     */
    static inline thread_local std::atomic<Batch<K, V>*> newBatchPtr{nullptr};
    PAD;
    /// @brief The atomic pointer to the top of the global, shared stack.
    std::atomic<nodeptr> main_top;
    PAD;
    /// @brief The array of aggregators.
    Aggregator<K, V> aggregator[NUMBER_AGGREGATORS];
    PAD;
    /// @brief Pointer to the memory reclamation manager.
    RecMgr* const recmgr;
    PAD;
    /// @brief Array to track thread initialization status.
    int init[MAX_THREADS_POW2] = {
        0,
    };
    PAD;

    /**
     * @brief Allocates and initializes a new Batch object.
     * @return A pointer to the newly created Batch.
     */
    __attribute__((noinline)) struct Batch<K, V>* CreateNewBatch() {
        struct Batch<K, V>* newBatch;
        newBatch = new Batch<K, V>;
        newBatch->popCounter.store(0, std::memory_order_relaxed);
        newBatch->pushCounter.store(0, std::memory_order_relaxed);
        newBatch->finalPopCount.store(0, std::memory_order_relaxed);
        newBatch->finalPushCount.store(0, std::memory_order_relaxed);
        newBatch->hasLeader.clear(std::memory_order_relaxed);
        newBatch->isBatchApplied.store(false, std::memory_order_relaxed);
        newBatch->subStackTop.store(NULL, std::memory_order_relaxed);

        // Allocate and initialize the elimination array
        newBatch->eliminationArray = (std::atomic<nodeptr>*)operator new[](
            sizeof(std::atomic<nodeptr>) * MAX_AGGREGATOR_THREADS);
        for (int i = 0; i < MAX_AGGREGATOR_THREADS; ++i) {
            new (&newBatch->eliminationArray[i])
                std::atomic<nodeptr>(nullptr);  // placement new
        }
        return newBatch;
    }

    /// @brief Reads the Time-Stamp Counter.
    inline uint64_t rdtsc() {
        unsigned int hi, lo;
        __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
        return ((uint64_t)lo) | (((uint64_t)hi) << 32);
    }

    /**
     * @brief Generates a small, non-deterministic number for backoff.
     * @return uint64_t A small random-ish number.
     */
    // inline uint64_t hwrand() { return 100 + (rdtsc() % 200); }

    /**
     * @brief Freezes the current batch, making it immutable.
     *
     * This function is called by the single elected 'freezer' thread for the
     * batch.
     *
     * @param aggregator The aggregator owning the batch.
     * @param batch The batch to be frozen.
     * @param tid The thread ID of the leader.
     */
    __attribute__((noinline)) void FreezeBatch(
        struct Aggregator<K, V>* aggregator,
        std::atomic<struct Batch<K, V>*> batch, const int& tid) {

        volatile int dummy = 0;
        for (int i = 0; i < 600; i++) {
            dummy++;
        }

        batch.load(std::memory_order_acquire)
            ->finalPushCount.store(
                batch.load(std::memory_order_acquire)
                    ->pushCounter.load(std::memory_order_acquire),
                std::memory_order_release);

        batch.load(std::memory_order_acquire)
            ->finalPopCount.store(
                batch.load(std::memory_order_acquire)
                    ->popCounter.load(std::memory_order_acquire),
                std::memory_order_release);

        // Atomically publish the *new* batch. This store signals all
        // waiting threads that the old batch is frozen and
        // they can proceed to the elimination/combining phase.
        aggregator->batch.store(newBatchPtr);

        newBatchPtr.store(CreateNewBatch(), std::memory_order_relaxed);
    }

    /**
     * @brief Creates and applies the 'sub-stack' of un-eliminated pushes.
     *
     * Called only by the 'push-combiner' thread (the thread with
     * `pushIndex == finalPopCount`).
     *
     * @param batch The frozen batch.
     * @param leaderIndex The index of the combiner thread
     * (which is `finalPopCount`).
     */
    __attribute__((noinline)) void CreatePushSubstackAndPush(
        struct Batch<K, V>* batch, int leaderIndex) {
        // Start with the leader's own node.
        struct node_t<K, V>* tempTop =
            batch->eliminationArray[leaderIndex].load();
        struct node_t<K, V>* tempBot = tempTop;
        int i = 1;

        // Iterate through all other un-eliminated pushes.
        while (i < (batch->finalPushCount.load(std::memory_order_acquire) -
                    batch->finalPopCount.load(std::memory_order_acquire))) {
            // Wait for the pusher thread to write its node.
            while (!batch->eliminationArray[leaderIndex + i].load()) {
            }
            nodeptr tempNode = batch->eliminationArray[leaderIndex + i].load(
                std::memory_order_relaxed);
            // Link it to our local sub-stack.
            tempNode->next = tempTop;
            tempTop = tempNode;
            i++;
        }

        // Atomically push the entire sub-stack onto the main stack top.
        while (true) {
            struct node_t<K, V>* top = main_top;  // Read current global top
            tempBot->next = top;  // Link bottom of our sub-stack to global top
            // Try to CAS the global top to the top of our sub-stack
            if (main_top.compare_exchange_strong(top, tempTop)) return;
            // Retry if CAS fails
        }
    }

   public:
    /**
     * @brief Constructs a new Stack.
     *
     * @param _num_threads Total number of threads that will use the stack.
     * @param _min_key (unused)
     * @param _max_key (unused)
     * @param _NO_VALUE (unused)
     * @param id (unused)
     */
    Stack(const int _num_threads, const int _min_key, const int _max_key,
          const V _NO_VALUE, unsigned int id)
        : main_top(NULL), recmgr(new RecMgr(_num_threads)) {
        // Calculate how many threads share a single aggregator.
        MAX_AGGREGATOR_THREADS = ceil(float(_num_threads) / NUMBER_AGGREGATORS);

        // Pre-allocate the first batch for each aggregator.
        for (int i = 0; i < NUMBER_AGGREGATORS; i++) {
            struct Batch<K, V>* first_batch = CreateNewBatch();
            aggregator[i].batch = first_batch;
        }
    }

    /**
     * @brief Destroys the Stack and frees all remaining nodes.
     */
    ~Stack() {
        recmgr->printStatus();
        // Free the main stack
        nodeptr curr = main_top.load(std::memory_order_relaxed);
        int cntr = 0;
        while (curr) {
            nodeptr next = curr->next.load(std::memory_order_relaxed);
            delete curr;
            curr = next;
            ++cntr;
        }
        main_top.store(nullptr, std::memory_order_relaxed);

        // Clean up aggregator batches
        for (int i = 0; i < NUMBER_AGGREGATORS; i++) {
            struct Batch<K, V>* batch = aggregator[i].batch.load();
            if (batch) delete batch;
        }

        // Clean up the main thread's thread-local newBatchPtr
        delete newBatchPtr;
        delete recmgr;
    }

    /**
     * @brief Returns the top value without popping it (unsafe in
     * concurrent context).
     * @param tid The thread ID.
     * @return V The value at the top, or default V if empty.
     */
    V peek(const int& tid) { return main_top ? main_top.load() : V(); }

    /**
     * @brief Pushes a value onto the stack.
     *
     * @param tid The ID of the calling thread.
     * @param value The value to push.
     * @return true always (stack is unbounded).
     */
    bool push(const int& tid, const V& value) {
        // Signal start of operation to memory reclaimer
        recmgr->startOp(tid);
        bool amICombiner = false;
        bool amIFreezer = false;
        Aggregator<K, V>* myAggregator = &CHOOSE_AGGREGATOR(tid);

        // Allocate node
        nodeptr myNode = recmgr->template allocate<node_t<K, V>>(tid);
        myNode->next = 0;
        myNode->val = value;

        while (true) {
            amICombiner = false;
            amIFreezer = false;

            // 1. Get current batch and announce your operation
            struct Batch<K, V>* myBatch = myAggregator->batch;
            int pushIndex = myBatch->pushCounter.fetch_add(1);

            // 2. Publish my node into the elimination array
            myBatch->eliminationArray[pushIndex].store(myNode);

            // 3. Freezer Election
            if (pushIndex == 0 && !myBatch->hasLeader.test_and_set()) {
                amIFreezer = true;
                // 4. Freezer Path: Freeze the batch
                FreezeBatch(myAggregator, myBatch, tid);
            } else {
                // 5. Follower Path: Spin until the batch is frozen
                int spin = 0;
                while (myBatch == myAggregator->batch) {
                }
            }

            // --- Batch is now frozen, proceed with local work ---

            // 6. Inclusion Check: Was I too late for this batch?
            if (pushIndex >= myBatch->finalPushCount.load()) {
                continue;  // Retry in the next batch
            }

            // 7. Elimination Check: Was I eliminated by a pop?
            if (pushIndex <
                myBatch->finalPopCount.load(std::memory_order_acquire)) {
                // Yes, my push was eliminated. My work is done.
                if (amIFreezer &&
                    (myBatch->finalPushCount.load(std::memory_order_acquire) ==
                     myBatch->finalPopCount.load(std::memory_order_acquire))) {
#ifdef BATCH_RETIRE
                    recmgr->retire(tid, myBatch);
#endif
                }
                recmgr->endOp(tid);
                return true;
            }

            // 8. Combiner Check: Am I the first non-eliminated push?
            if (pushIndex ==
                myBatch->finalPopCount.load(std::memory_order_acquire)) {
                // 9. Combiner Path: Build sub-stack and apply to main_top
                CreatePushSubstackAndPush(myBatch, pushIndex);
                // Signal followers that the work is done
                myBatch->isBatchApplied.store(true, std::memory_order_release);
                amICombiner = true;
            } else {
                // 10. Follower Path: Wait for combiner to finish
                int spin = 0;
                while (myBatch->isBatchApplied.load() == false) {
                }
            }

            // 11. Cleanup
#ifdef BATCH_RETIRE
            if (amICombiner) recmgr->retire(tid, myBatch);
#endif
            recmgr->endOp(tid);
            return true;
        }
    }

    /**
     * @brief Atomically pops a sub-stack of `popCount` nodes from `main_top`.
     *
     * Called only by the 'pop-combiner' thread.
     *
     * @param popCount The number of nodes to pop.
     * @return nodeptr The head of the popped sub-stack.
     */
    nodeptr PopFromMain(int popCount) {
        while (true) {
            nodeptr top = main_top;  // Read current top
            nodeptr temp = top;

            // Traverse down `popCount` nodes
            for (size_t i = 0; i < popCount; i++) {
                if (temp == NULL) break;  // Stack has fewer nodes than popCount
                temp = temp->next;
            }

            // Try to CAS main_top to the new top (temp)
            if (main_top.compare_exchange_strong(top, temp)) {
                return top;  // Return the substack we just popped
            }
            // Retry on CAS failure
        }
    }

    /**
     * @brief Helper for pops to get their return value from the popped
     * sub-stack.
     *
     * @param index The thread's offset into the sub-stack.
     * @param top The head of the popped sub-stack (from `subStackTop`).
     * @param tid The thread ID.
     * @return V The value from the corresponding node.
     */
    V GetRetValue(int index, nodeptr top, const int& tid) {
        if (top == NULL) {
            return V();  // Stack was empty
        }
        nodeptr temp = top;
        // Walk the list to find our node
        for (size_t i = 0; i < index; i++) {
            temp = temp->next;
            if (temp == NULL) {
                return V();  // Return empty if the number of popped nodes is
                             // less than index
            }
        }

        V res = temp->val;
        recmgr->retire(tid, temp);  // Retire the node
        return res;
    }

    /**
     * @brief Pops a value from the stack.
     *
     * @param tid The ID of the calling thread.
     * @return V The popped value, or default V if empty.
     */
    bool pop(const int& tid) {
        recmgr->startOp(tid);
        bool amICombiner = false;
        bool amIFreezer = false;
        Aggregator<K, V>* myAggregator = &CHOOSE_AGGREGATOR(tid);
        bool success = false;

        while (true) {
            amICombiner = false;
            amIFreezer = false;

            // 1. Get current batch and claim a slot
            struct Batch<K, V>* myBatch = myAggregator->batch;
            int popIndex = myBatch->popCounter.fetch_add(1);

            // 2. Freezer Election
            if (popIndex == 0 && !myBatch->hasLeader.test_and_set()) {
                amIFreezer = true;
                // 3. Freezer Path: Freeze the batch
                FreezeBatch(myAggregator, myBatch, tid);
            } else {
                // 4. Follower Path: Spin until the batch is frozen
                int spin = 0;
                while (myBatch == myAggregator->batch) {
                }
            }

            // --- Batch is now frozen, proceed with local work ---

            // 5. Inclusion Check: Was I too late for this batch?
            if (popIndex >= myBatch->finalPopCount.load()) {
                continue;  // Retry in the next batch
            }

            // 6. Elimination Check: Was I eliminated by a push?
            if (popIndex <
                myBatch->finalPushCount.load(std::memory_order_acquire)) {
                // 7. Elimination Path: Wait for my matching push
                while (!myBatch->eliminationArray[popIndex].load()) {
                    // Wait for Push to write value.
                }

                // Get value from the pushes node
                nodeptr my_ptr = myBatch->eliminationArray[popIndex].load();
                V returnValue = my_ptr->val;

                // Handle batch retirement if I'm the freezer and batch was
                // perfectly eliminated
                if (amIFreezer &&
                    (myBatch->finalPushCount.load(std::memory_order_acquire) ==
                     myBatch->finalPopCount.load(std::memory_order_acquire))) {
#ifdef BATCH_RETIRE
                    recmgr->retire(tid, myBatch);
#endif
                }
                recmgr->endOp(tid);
                return returnValue;
            }

            // --- Not eliminated, must pop from main stack ---

            // 8. Combiner Check: Am I the first non-eliminated pop?
            if (popIndex ==
                myBatch->finalPushCount.load(std::memory_order_acquire)) {
                // 9. Combiner Path:
                amICombiner = true;
                // Calculate how many nodes to pop from main_top
                int remainingPops =
                    myBatch->finalPopCount.load(std::memory_order_acquire) -
                    myBatch->finalPushCount.load(std::memory_order_acquire);

                // Atomically pop the sub-stack
                myBatch->subStackTop.store(PopFromMain(remainingPops),
                                           std::memory_order_release);
                // Signal followers
                myBatch->isBatchApplied.store(true, std::memory_order_release);
            } else {
                // 10. Follower Path: Wait for combiner
                int spin = 0;
                while (myBatch->isBatchApplied.load(
                           std::memory_order_acquire) == false) {
                }
            }

            // 11. All Non-eliminated Pops: Get my value from the sub-stack
            V res = GetRetValue(
                popIndex -  // My index within the *popped* sub-stack
                    myBatch->finalPushCount.load(std::memory_order_acquire),
                myBatch->subStackTop.load(std::memory_order_acquire), tid);

            // 12. Cleanup
#ifdef BATCH_RETIRE
            if (amICombiner) {
                recmgr->retire(tid, myBatch);
            }
#endif
            recmgr->endOp(tid);
            return res;
        }
        return true;
    }

    RecMgr* debugGetRecMgr() { return recmgr; }

    /**
     * @brief Initializes thread-local data.
     * Must be called by each thread before it uses the stack.
     * @param tid The thread's unique ID.
     */
    void initThread(const int tid) {
        // Pre-allocate the first "next" batch for this thread
        newBatchPtr.store(CreateNewBatch(), std::memory_order_relaxed);
        if (init[tid])
            return;
        else
            init[tid] = !init[tid];
        recmgr->initThread(tid);
    }

    /**
     * @brief De-initializes thread-local data.
     * @param tid The thread's unique ID.
     */
    void deinitThread(const int tid) {
        if (!init[tid])
            return;
        else
            init[tid] = !init[tid];
        recmgr->deinitThread(tid);
    }
};

#endif