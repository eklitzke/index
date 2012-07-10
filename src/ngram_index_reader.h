// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_NGRAM_INDEX_READER_H_
#define SRC_NGRAM_INDEX_READER_H_

#include <set>
#include <string>

#include "./integer_index_reader.h"
#include "./search_results.h"

namespace codesearch {
class NGramIndexReader {
 public:
  NGramIndexReader(const std::string &index_directory, std::size_t ngram_size);

  // Find a string in the ngram index. This should be the full query,
  // like "struct foo" or "madvise". If limit is non-zero, it is the
  // max number of results that will be returned.
  void Find(const std::string &query, SearchResults *results);

  ~NGramIndexReader();

 private:
  const std::size_t ngram_size_;
  const IntegerIndexReader files_index_;
  const IntegerIndexReader lines_index_;
  std::vector<SSTableReader*> shards_;

  void FindShard(const std::string &query,
                 const std::set<std::string> &ngrams,
                 const SSTableReader &reader,
                 SearchResults *results);

  bool GetCandidates(const std::string &ngram,
                     std::vector<std::uint64_t> *candidates,
                     const SSTableReader &reader,
                     std::size_t *lower_bound);

};
}  // codesearch

#endif  // SRC_NGRAM_INDEX_READER_H_
