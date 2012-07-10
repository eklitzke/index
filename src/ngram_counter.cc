// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./ngram_counter.h"

#include <algorithm>

namespace {
codesearch::NGramCounter *counter_ = nullptr;
}

namespace codesearch {

NGramCounter* NGramCounter::Instance() {
  if (counter_ == nullptr) {
    counter_ = new NGramCounter;
  }
  return counter_;
}

void NGramCounter::UpdateCount(const std::string &ngram, std::size_t count) {
  std::lock_guard<std::mutex> guard(mutex_);
  counts_[ngram] += count;
}

NGramCounts NGramCounter::ReverseSortedCounts() {
  std::vector<std::pair<std::size_t, std::string> > reverse_counts;
  for (const auto &p : counts_) {
    reverse_counts.push_back(std::make_pair(p.second, p.first));
  }
  std::sort(reverse_counts.begin(), reverse_counts.end());
  std::reverse(reverse_counts.begin(), reverse_counts.end());

  NGramCounts counts;
  for (const auto &p : reverse_counts) {
    NGramCount *count = counts.add_ngram_counts();
    assert(count != nullptr);
    count->set_ngram(p.second);
    count->set_count(p.first);
  }
  return counts;
}
}
