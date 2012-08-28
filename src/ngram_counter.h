// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>
//
// This file contains the utility class used to store ngram
// counts. This is ultimately used to generate a sorted count list
// that is used for one-gram and two-gram queries.

#ifndef SRC_NGRAM_COUNTER_H_
#define SRC_NGRAM_COUNTER_H_

#include "./index.pb.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace codesearch {
class NGramCounter {
 public:
  // Get an instance of the NGramCounter.
  static NGramCounter* Instance();

  // Update the count for an ngram.
  void UpdateCount(const std::string &ngram, std::size_t count);

  // Update the count for multiple ngrams (this is more efficient than
  // calling UpdateCount() in a loop).
  void UpdateCounts(const std::map<std::string, std::size_t> &count_map);

  // The same, but where we have a map to a vector instead of a map to
  // a count.
  template <typename T>
  void UpdateCounts(const std::map<std::string, std::vector<T> > &list_map) {
    std::map<std::string, std::size_t> count_map;
    for (const auto &kv : list_map) {
      count_map.insert(
          count_map.end(), std::make_pair(kv.first, kv.second.size()));
    }

    // Delegate to the function mapping strings to counts.
    UpdateCounts(count_map);
  }

  // Get the reverse sorted count list.
  NGramCounts ReverseSortedCounts();

 private:
  std::mutex mutex_;
  std::unordered_map<std::string, std::size_t> counts_;

  NGramCounter() {}
};

// Context manager that ensures that the NGramCounter object is
// deleted once the context manager goes out of scope.
class NGramCounterCtx {
 public:
  NGramCounterCtx() :ctx_ptr_(NGramCounter::Instance()) {}
 private:
  std::unique_ptr<NGramCounter> ctx_ptr_;
};
}

#endif
