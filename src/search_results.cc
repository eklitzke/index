// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./search_results.h"

namespace codesearch {
void SearchResults::Reset() {
  std::lock_guard<std::mutex> guard(mutex_);
  results_.clear();
}

void SearchResults::SetCapacity(std::size_t capacity) {
  std::lock_guard<std::mutex> guard(mutex_);
  capacity_ = capacity;
  if (capacity_ && results_.size() > capacity_) {
    results_.erase(results_.begin() + capacity_, results_.end());
  }
}

bool SearchResults::IsFull() {
  std::lock_guard<std::mutex> guard(mutex_);
  return capacity_ && results_.size() >= capacity_;
}

bool SearchResults::AddResult(const std::string &filename, std::size_t line_num,
                              const std::string &line_text) {
  std::lock_guard<std::mutex> guard(mutex_);
  if (!capacity_ || results_.size() < capacity_) {
    SearchResult r;
    r.set_filename(filename);
    r.set_line_num(line_num);
    r.set_line_text(line_text);
    results_.push_back(r);
    return true;
  }
  return false;
}

void SearchResults::Extend(SearchResults *other) {
  std::unique_lock<std::mutex> lhs_lock(mutex_, std::defer_lock);
  std::unique_lock<std::mutex> rhs_lock(other->mutex_, std::defer_lock);
  std::lock(lhs_lock, rhs_lock);

  if (!capacity_ || results_.size() < capacity_) {
    const auto &other_results = other->results();
    results_.insert(results_.end(), other_results.begin(),
                    other_results.end());
    if (capacity_ && results_.size() > capacity_) {
      results_.erase(results_.begin() + capacity_, results_.end());
    }
  }
}
}
