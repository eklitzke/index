// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_NGRAM_COUNTER_H_
#define SRC_NGRAM_COUNTER_H_

#include <unordered_map>
#include <mutex>
#include <string>
#include <vector>

#include "./index.pb.h"
#include "./ngram.h"

namespace codesearch {
class NGramCounter {
 public:
  // Update the count for an NGram
  void UpdateCount(const NGram &ngram, std::uint64_t count);

  // Get the reverse sorted counts, that is, the map of {ngram: count}
  // ordered by most common ngram first.
  NGramCounts ReverseSortedCounts();

  // Get the total count of all ngram counts, i.e. this is like
  // sum(counts_.values()).
  std::uint64_t TotalCount();

  // Get the total number of ngrams, i.e. this is like len(counts_.keys())
  std::uint64_t TotalNGrams();

 private:
  std::mutex mutex_;
  std::unordered_map<NGram, std::uint64_t> counts_;
};
}

#endif  // SRC_NGRAM_COUNTER_H_
