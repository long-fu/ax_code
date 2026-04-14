#ifndef THREAD_SAFE_QUEUE_H
#define THREAD_SAFE_QUEUE_H

#include <mutex>
#include <queue>

template<typename T>
class ThreadSafeQueue {
public:

    /**
     * @brief ThreadSafeQueue constructor
     * @param [in] capacity: the queue capacity
     */
    ThreadSafeQueue(uint32_t capacity)
    {
        // check the input value: capacity is valid
        if (capacity >= kMinQueueCapacity && capacity <= kMaxQueueCapacity) {
            m_iQueueCapacity = capacity;
        } else { // the input value: capacity is invalid, set the default value
            m_iQueueCapacity = kDefaultQueueCapacity;
        }
    }

    /**
     * @brief ThreadSafeQueue constructor
     */
    ThreadSafeQueue()
    {
        m_iQueueCapacity = kDefaultQueueCapacity;
    }

    /**
     * @brief ThreadSafeQueue destructor
     */
    ~ThreadSafeQueue() = default;

    /**
     * @brief push data to queue
     * @param [in] input_value: the value will push to the queue
     * @return true: success to push data; false: fail to push data
     */
    bool Push(T input_value)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // check current size is less than capacity
        if (m_queue.size() < m_iQueueCapacity) {
            m_queue.push(input_value);
            return true;
        }
        fprintf(stderr,"缓存队列消息满\n");
        return false;
    }

    /**
     * @brief pop data from queue
     * @return true: success to pop data; false: fail to pop data
     */
    T Pop()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.empty()) { // check the queue is empty
            // fprintf(stderr,"消息队列消息没有\n");
            return nullptr;
        }

        T tmp_ptr = m_queue.front();
        m_queue.pop();
        return tmp_ptr;
    }

    /**
     * @brief check the queue is empty
     * @return true: the queue is empty; false: the queue is not empty
     */
    bool Empty()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }

    /**
     * @brief get the queue size
     * @return the queue size
     */
    uint32_t Size()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }

    void ExtendCapacity(uint32_t newSize)
    {
        m_iQueueCapacity = newSize;
        kMaxQueueCapacity = newSize >kMaxQueueCapacity ? newSize : kMaxQueueCapacity;
    }

private:
    std::queue<T> m_queue; // the queue
    uint32_t m_iQueueCapacity; // queue capacity
    mutable std::mutex m_mutex; // the mutex value
    const uint32_t kMinQueueCapacity = 1; // the minimum queue capacity
    const uint32_t kMaxQueueCapacity = 10000; // the maximum queue capacity
    const uint32_t kDefaultQueueCapacity = 10; // default queue capacity
};

#endif /* THREAD_SAFE_QUEUE_H */