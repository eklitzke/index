// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_SEARCH_RESULTS_H_
#define SRC_SEARCH_RESULTS_H_

#include <mutex>
#include <string>
#include <vector>

#include "./index.pb.h"

namespace codesearch {

struct FileResult {
  FileResult(std::size_t off, std::size_t line_num)
      :offset(off), line_number(line_num) {}
  std::size_t offset;
  std::size_t line_number;
};

class SearchResults {
 public:
  // Create a container for search results.
  //
  // Args:
  //  limit, the maximum number of files to match
  //  within_file_limit, the maximum number of results to match within
  //                     a file
  //  offset, the offset to use
  explicit SearchResults(std::size_t limit, std::size_t within_file_limit = 10,
                         std::size_t offset = 0)
      :capacity_(limit), within_file_limit_(within_file_limit),
       offset_(offset) {}

  // Is this container full? This is used so the search threads can
  // know when they can stop working.
  bool IsFull();

  // Trim the result set.
  void Trim();

  bool AddFileResult(const std::string &filename,
                     const std::vector<FileResult> &results);

  std::size_t size() {
    std::lock_guard<std::mutex> guard(mutex_);
    return results_.size();
  }


  std::size_t max_within_file() const {
    return within_file_limit_;
  }

  std::vector<SearchResultContext> contextual_results();

  // The mutex is publicly visible.
  std::mutex mutex_;

 private:
  const std::size_t capacity_;
  const std::size_t within_file_limit_;
  const std::size_t offset_;
  std::map<std::string, std::vector<FileResult> > results_;

  bool UnlockedIsFull() {
    return capacity_ && results_.size() >= (capacity_ + offset_);
  }
};
}

#endif
