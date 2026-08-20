#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace my_utils {

// Fixed-size worker pool with bounded queue.
// Submit returns nullopt when the queue is full (non-blocking).
class ThreadPool {
 public:
  explicit ThreadPool(size_t thread_count = 2, size_t max_queue = 128)
      : max_queue_(max_queue == 0 ? 1 : max_queue) {
    if (thread_count == 0) {
      thread_count = 1;
    }
    workers_.reserve(thread_count);
    for (size_t i = 0; i < thread_count; ++i) {
      workers_.emplace_back([this] { WorkerLoop(); });
    }
  }

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  ~ThreadPool() { Shutdown(); }

  void Shutdown() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (stop_) {
        return;
      }
      stop_ = true;
    }
    cv_.notify_all();
    for (auto& t : workers_) {
      if (t.joinable()) {
        t.join();
      }
    }
    workers_.clear();
  }

  size_t Pending() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tasks_.size();
  }

  template <class F, class... Args>
  auto Submit(F&& f, Args&&... args)
      -> std::optional<std::future<std::invoke_result_t<F, Args...>>> {
    using R = std::invoke_result_t<F, Args...>;

    auto task = std::make_shared<std::packaged_task<R()>>(
        [fn = std::decay_t<F>(std::forward<F>(f)),
         tup = std::make_tuple(std::forward<Args>(args)...)]() mutable -> R {
          return std::apply(std::move(fn), std::move(tup));
        });

    std::future<R> fut = task->get_future();
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (stop_ || tasks_.size() >= max_queue_) {
        return std::nullopt;
      }
      tasks_.emplace([task]() { (*task)(); });
    }
    cv_.notify_one();
    return fut;
  }

 private:
  void WorkerLoop() {
    for (;;) {
      std::function<void()> job;
      {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
        if (stop_ && tasks_.empty()) {
          return;
        }
        job = std::move(tasks_.front());
        tasks_.pop();
      }
      job();
    }
  }

  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::queue<std::function<void()>> tasks_;
  std::vector<std::thread> workers_;
  size_t max_queue_;
  bool stop_ = false;
};

}  // namespace my_utils
