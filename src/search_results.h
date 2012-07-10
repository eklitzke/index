// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_SEARCH_RESULTS_H_
#define SRC_SEARCH_RESULTS_H_

#include <mutex>
#include <string>
#include <vector>

#include "./index.pb.h"

namespace codesearch {

class SearchResults {
 public:
  SearchResults() :capacity_(0) {}
  explicit SearchResults(std::size_t capacity)
      :capacity_(capacity) {}

  void Reset();

  void SetCapacity(std::size_t capacity);

  bool IsFull();

  // Add a result; returns true if the result was added, false if the
  // container was full (and hence the result was not added).
  bool AddResult(const std::string &filename, std::size_t line_num,
                 const std::string &line_text);

  void Extend(SearchResults *other);

  std::size_t size() {
    std::lock_guard<std::mutex> guard(mutex_);
    return results_.size();
  }


  // None of these are locked! Exercise caution when using these.
  const std::vector<SearchResult>& results() const {return results_; };

  // The mutex is publicly visible.
  std::mutex mutex_;

 private:
  std::size_t capacity_;
  std::vector<SearchResult> results_;
};
}

#endif
