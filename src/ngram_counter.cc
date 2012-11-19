// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./ngram_counter.h"

#include <algorithm>

namespace {
codesearch::NGramCounter *counter_ = nullptr;

struct ReverseCount {
  codesearch::NGram ngram;
  std::uint64_t count;

  ReverseCount(const codesearch::NGram &n, std::uint64_t c)
      :ngram(n), count(c) {}

  inline bool operator<(const ReverseCount &other) const {
    return other.count < count;
  }
};
}

namespace codesearch {

NGramCounter* NGramCounter::Instance() {
  if (counter_ == nullptr) {
    counter_ = new NGramCounter;
  }
  return counter_;
}

void NGramCounter::UpdateCount(const NGram &ngram, std::uint64_t count) {
  std::lock_guard<std::mutex> guard(mutex_);
  counts_[ngram] += count;
}

NGramCounts NGramCounter::ReverseSortedCounts() {
  std::vector<ReverseCount> reverse_counts;
  for (const auto &p : counts_) {
    reverse_counts.push_back(ReverseCount(p.first, p.second));
  }
  std::sort(reverse_counts.begin(), reverse_counts.end());

  NGramCounts counts;
  for (const auto &p : reverse_counts) {
    NGramCount *count = counts.add_ngram_counts();
    assert(count != nullptr);
    count->set_ngram(p.ngram.string());
    count->set_count(p.count);
  }
  return counts;
}

std::uint64_t NGramCounter::TotalCount() {
  std::uint64_t total = 0;
  for (const auto &kv : counts_) {
    total += kv.second;
  }
  return total;
}

std::uint64_t NGramCounter::TotalNGrams() {
  return counts_.size();
}
}
