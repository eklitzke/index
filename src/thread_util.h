// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_THREAD_UTIL_H_
#define SRC_THRAD_UTIL_H_

#include <condition_variable>
#include <mutex>
#include <thread>

namespace codesearch {

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
