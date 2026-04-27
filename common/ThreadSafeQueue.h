#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>

#include "Logger.h"

template <typename T>
class ThreadSafeQueue {
 public:
  explicit ThreadSafeQueue(uint32_t capacity)
      : m_queue_capacity_(capacity) {}

  ThreadSafeQueue() : m_queue_capacity_(kDefaultQueueCapacity) {}

  ~ThreadSafeQueue() = default;

  bool Push(T input_value) {
    std::lock_guard<std::mutex> lock(m_mutex_);
    if (m_queue_.size() < m_queue_capacity_) {
      m_queue_.push(input_value);
      return true;
    }
    fprintf(stderr, "缓存队列消息满\n");
    return false;
  }

  T Pop() {
    std::lock_guard<std::mutex> lock(m_mutex_);
    if (m_queue_.empty()) {
      return nullptr;
    }
    T tmp_ptr = m_queue_.front();
    m_queue_.pop();
    return tmp_ptr;
  }

  bool Empty() {
    std::lock_guard<std::mutex> lock(m_mutex_);
    return m_queue_.empty();
  }

  uint32_t Size() {
    std::lock_guard<std::mutex> lock(m_mutex_);
    return m_queue_.size();
  }

  void ExtendCapacity(uint32_t new_size) {
    m_queue_capacity_ = new_size;
  }

 private:
  std::queue<T> m_queue_;
  uint32_t m_queue_capacity_ = kDefaultQueueCapacity;
  mutable std::mutex m_mutex_;

  static constexpr uint32_t kMinQueueCapacity = 1;
  static constexpr uint32_t kMaxQueueCapacity = 10000;
  static constexpr uint32_t kDefaultQueueCapacity = 10;
};