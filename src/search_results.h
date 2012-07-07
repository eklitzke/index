// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_SEARCH_RESULTS_H_
#define SRC_SEARCH_RESULTS_H_

#include <string>
#include <vector>
#include "./index.pb.h"

namespace codesearch {

class SearchResults {
 public:
  SearchResults() :capacity_(0), attempted_(0) {}
  explicit SearchResults(std::size_t capacity)
      :capacity_(capacity), attempted_(0) {}
  void Reset() { results_.clear(); attempted_ = 0; }

  void SetCapacity(std::size_t capacity) {
    capacity_ = capacity;
    if (capacity_ && results_.size() > capacity_) {
      results_.erase(results_.begin() + capacity_, results_.end());
    }
  }

  bool IsFull() {
    return capacity_ && results_.size() >= capacity_;
  }

  void AddResult(const std::string &filename, std::size_t line_num,
                 const std::string &line_text) {
    attempted_++;
    if (!capacity_ || results_.size() < capacity_) {
      SearchResult r;
      r.set_filename(filename);
      r.set_line_num(line_num);
      r.set_line_text(line_text);
      results_.push_back(r);
    }
  }

  void Extend(const SearchResults &other) {
    attempted_ += other.attempted();
    if (!capacity_ || results_.size() < capacity_) {
      const auto &other_results = other.results();
      results_.insert(results_.end(), other_results.begin(),
                      other_results.end());
      if (capacity_ && results_.size() > capacity_) {
        results_.erase(results_.begin() + capacity_, results_.end());
      }
    }
  }

  const std::vector<SearchResult>& results() const { return results_; };
  std::size_t attempted() const { return attempted_; }
  std::size_t size() const { return results_.size(); }
 private:
  std::size_t capacity_;
  std::size_t attempted_;
  std::vector<SearchResult> results_;
};
}

#endif
