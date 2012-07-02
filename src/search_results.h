// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_SEARCH_RESULTS_H_
#define SRC_SEARCH_RESULTS_H_

#include <string>
#include <vector>

namespace codesearch {

class SearchResult {
 public:
  SearchResult(const std::string &filename, std::size_t line_num,
               const std::string &line_text)
      :filename_(filename), line_num_(line_num), line_text_(line_text) {}
  const std::string& filename() const { return filename_; }
  const std::size_t& line_num() const { return line_num_; }
  const std::string& line_text() const { return line_text_; }
 private:
  std::string filename_;
  std::size_t line_num_;
  std::string line_text_;
};

class SearchResults {
 public:
  SearchResults() {}
  void Reset() { results_.clear(); }
  void AddResult(const std::string &filename, std::size_t line_num,
                 const std::string &line_text) {
    SearchResult r(filename, line_num, line_text);
    results_.push_back(r);
  }

  void Extend(const SearchResults &other) {
    const auto &other_results = other.results();
    results_.insert(results_.end(), other_results.begin(), other_results.end());
  }

  const std::vector<SearchResult>& results() const { return results_; };
 private:
  std::vector<SearchResult> results_;
};
}

#endif
