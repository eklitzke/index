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
  SearchResults(std::size_t capacity, std::size_t offset)
      :capacity_(capacity), offset_(offset), cur_offset_(0) {}
  explicit SearchResults(std::size_t capacity)
      :SearchResults(capacity, 0) {}
  SearchResults()
      :SearchResults(0, 0) {}

  bool IsFull();

  // Add a result; returns true if the result was added, false if the
  // container was full (and hence the result was not added).
  bool AddResult(const std::string &filename, std::size_t line_num,
                 std::size_t line_offset, const std::string &line_text);

  std::size_t size() {
    std::lock_guard<std::mutex> guard(mutex_);
    return results_.size();
  }


  // None of these are locked! Exercise caution when using these.
  const std::vector<SearchResult>& results() const {return results_; };

  std::vector<SearchResultContext> contextual_results();

  // The mutex is publicly visible.
  std::mutex mutex_;

 private:
  const std::size_t capacity_;
  const std::size_t offset_;
  std::size_t cur_offset_;
  std::vector<SearchResult> results_;
};
}

#endif
