// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>
//
// A simple thread-safe queue.

#ifndef SRC_QUEUE_H_
#define SRC_QUEUE_H_

#include <condition_variable>
#include <mutex>
#include <queue>

namespace codesearch {
// A standard threadsafe queue
template <typename T>
class Queue {
 public:
  explicit Queue(std::size_t max_size = 0) :max_size_(max_size) {}

  T pop() {
    std::unique_lock<std::mutex> guard(queue_mut_);
    if (!queue_.empty()) {
      T val = queue_.front();
      queue_.pop();
      return val;
    }
    cond_.wait(guard, [=](){ return !queue_.empty(); });
    assert(!queue_.empty());
    T val = queue_.front();
    queue_.pop();
    return val;
  }

  void push(T val) {
    std::unique_lock<std::mutex> guard(queue_mut_);
    if (max_size_ && queue_.size() >= max_size_) {
      cond_.wait(guard, [=](){ return queue_.size() <= max_size_; });
    }
    assert(!max_size_ || queue_.size() < max_size_);
    queue_.push(val);
    cond_.notify_all();
  }

  std::size_t size() const {
    std::unique_lock<std::mutex> guard(queue_mut_);
    return queue_.size();
  }

  bool empty() const {
    std::unique_lock<std::mutex> guard(queue_mut_);
    return queue_.empty();
  }

 private:
  std::condition_variable cond_;
  mutable std::mutex queue_mut_;
  std::queue<T> queue_;
  std::size_t max_size_;
};
}  // namespace codesearch

#endif  // SRC_QUEUE_H_
