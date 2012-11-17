// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_THREAD_UTIL_H_
#define SRC_THRAD_UTIL_H_

#include <cassert>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace codesearch {
// This is a helper class for waiting for an integer to reach some
// value, and then acquiring a lock; releasing the lock increments the
// internal counter. This is used to help enforce an ordering of how
// files/positions/ngrams are written out in the index writer.
class IntWait {
 public:
  explicit IntWait(std::size_t start = 0) :val_(start) {}
  IntWait(const IntWait &other) = delete;
  IntWait& operator=(const IntWait &other) = delete;

  class WaitHandle {
   public:
    WaitHandle(codesearch::IntWait *wait, std::size_t val)
        :wait_(wait), val_(val), need_release_(true) {
      wait_->Acquire(val);
    }
    WaitHandle(WaitHandle &&other)
        :wait_(other.wait_), val_(other.val_), need_release_(true) {
      assert(other.need_release_ == true);
      other.need_release_ = false;
    }
    WaitHandle(const WaitHandle &other) = delete;
    WaitHandle& operator=(const WaitHandle &other) = delete;

    ~WaitHandle() { if (need_release_) { wait_->Release(val_); } }
   private:
    codesearch::IntWait *wait_;
    std::size_t val_;
    bool need_release_;
  };

  // Get a handle to the IntWait, for a given val.
  WaitHandle Handle(std::size_t val) {
    return std::move(WaitHandle(this, val));
  }

 private:
  std::mutex mut_;
  std::size_t val_;
  std::condition_variable cond_;

  void Acquire(std::size_t val) {
    std::unique_lock<std::mutex> guard(mut_);
    cond_.wait(guard, [&]() { return val_ == val; });
  }

  void Release(std::size_t val) {
    std::lock_guard<std::mutex> guard(mut_);
    assert(val_ == val);
    val_++;
    cond_.notify_all();
  }
};

class FunctionThreadPool {
  enum class WorkState {
    IDLE     = 1,
    READY    = 2,
    STOPPED  = 3
  };

  class Worker {
   public:
    Worker(FunctionThreadPool *pool)
        :pool_(pool), state_(WorkState::IDLE) {}

    void Start() {
      while (true) {
        std::unique_lock<std::mutex> lock(mut_);
        cond_.wait(lock, [&]() { return state_ != WorkState::IDLE; });
        if (state_ == WorkState::STOPPED) {
          break;
        } else if (state_ == WorkState::READY) {
          func_();
          state_ = WorkState::IDLE;
          pool_->Notify(this);
        } else {
          assert(false);
        }
      }
    }

    void Stop() {
      std::lock_guard<std::mutex> guard(mut_);
      state_ = WorkState::STOPPED;
      cond_.notify_all();
    }

    void Send(std::function<void()> func) {
      std::lock_guard<std::mutex> guard(mut_);
      func_ = func;
      state_ = WorkState::READY;
      cond_.notify_all();
    }
   private:
    FunctionThreadPool *pool_;
    WorkState state_;
    std::function<void()> func_;

    std::mutex mut_;
    std::condition_variable cond_;
  };

 public:
  explicit FunctionThreadPool(std::size_t size)
      :waited_(false) {
    for (std::size_t i = 0; i < size; i++) {
      Worker *worker = new Worker(this);
      pool_.emplace_back(worker);
      threads_.insert(
          std::make_pair(worker, std::thread(
              std::bind(&Worker::Start, worker))));
    }
  }

  void Notify(Worker *worker) {
    std::lock_guard<std::mutex> guard(mut_);
    pool_.emplace_back(worker);
    cond_.notify_all();
  }

  void Send(std::function<void()> func) {
    std::unique_lock<std::mutex> lock(mut_);
    cond_.wait(lock, [&]() { return !pool_.empty(); });
    assert(!pool_.empty());
    pool_.back()->Send(func);
    pool_.pop_back();
  }

  void Wait() {
    std::unique_lock<std::mutex> lock(mut_);
    if (!waited_) {
      cond_.wait(lock, [&]() { return pool_.size() >= threads_.size(); });
      for (auto &worker : pool_) {
        worker->Stop();
      }
      for (auto &kv : threads_) {
        kv.second.join();
        delete kv.first;
      }
      waited_ = true;
    }
  }

  ~FunctionThreadPool() {
    Wait();
  }

 private:
  std::mutex mut_;
  std::condition_variable cond_;

  bool waited_;
  std::vector<Worker *> pool_;
  std::map<Worker *, std::thread> threads_;

};
}  // namespace codesearch
#endif  // SRC_UTIL_H_
