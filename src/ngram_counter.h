// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_NGRAM_COUNTER_H_
#define SRC_NGRAM_COUNTER_H_

#include "./index.pb.h"
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace codesearch {
class NGramCounter {
 public:
  static NGramCounter* Instance();

  void UpdateCount(const std::string &ngram, std::size_t count);

  NGramCounts ReverseSortedCounts();

 private:
  std::mutex mutex_;
  std::map<std::string, std::size_t> counts_;

  NGramCounter() {}
};
}

#endif
