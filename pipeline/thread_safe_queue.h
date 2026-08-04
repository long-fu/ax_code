#ifndef PIPELINE_THREAD_SAFE_QUEUE_H
#define PIPELINE_THREAD_SAFE_QUEUE_H

#include <cstdint>
#include <mutex>
#include <queue>

namespace pipeline {

template<typename T>
class ThreadSafeQueue {
public:
    ThreadSafeQueue(uint32_t capacity) {
        if (capacity >= kMinQueueCapacity && capacity <= kMaxQueueCapacity) {
            queue_capacity_ = capacity;
        } else {
            queue_capacity_ = kDefaultQueueCapacity;
        }
    }

    ThreadSafeQueue() {
        queue_capacity_ = kDefaultQueueCapacity;
    }

    ~ThreadSafeQueue() = default;

    bool Push(T input_value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.size() < queue_capacity_) {
            queue_.push(input_value);
            return true;
        }
        return false;
    }

    T Pop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return nullptr;
        }
        T tmp_ptr = queue_.front();
        queue_.pop();
        return tmp_ptr;
    }

    bool Empty() {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    uint32_t Size() {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    void ExtendCapacity(uint32_t new_size) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_capacity_ = new_size;
        kMaxQueueCapacity = new_size > kMaxQueueCapacity
                            ? new_size : kMaxQueueCapacity;
    }

private:
    std::queue<T> queue_;
    uint32_t queue_capacity_;
    mutable std::mutex mutex_;
    const uint32_t kMinQueueCapacity = 1;
    uint32_t kMaxQueueCapacity = 10000;
    const uint32_t kDefaultQueueCapacity = 10;
};

} // namespace pipeline
#endif  // PIPELINE_THREAD_SAFE_QUEUE_H
