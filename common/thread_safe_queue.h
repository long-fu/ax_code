#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>

#include "logger.h"

template <typename T>
class ThreadSafeQueue {
 public:
  explicit ThreadSafeQueue(uint32_t capacity)
      : queue_capacity_(capacity) {}

  ThreadSafeQueue() : queue_capacity_(kDefaultQueueCapacity) {}

  ~ThreadSafeQueue() = default;

  bool Push(T input_value) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.size() < queue_capacity_) {
      queue_.push(input_value);
      return true;
    }
    fprintf(stderr, "缓存队列消息满\n");
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
    queue_capacity_ = new_size;
  }

 private:
  std::queue<T> queue_;
  uint32_t queue_capacity_ = kDefaultQueueCapacity;
  mutable std::mutex mutex_;

  static constexpr uint32_t kMinQueueCapacity = 1;
  static constexpr uint32_t kMaxQueueCapacity = 10000;
  static constexpr uint32_t kDefaultQueueCapacity = 10;
};