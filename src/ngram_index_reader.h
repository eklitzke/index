// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_NGRAM_INDEX_READER_H_
#define SRC_NGRAM_INDEX_READER_H_

#include <condition_variable>
#include <set>
#include <string>

#include "./bounded_map.h"
#include "./context.h"
#include "./integer_index_reader.h"
#include "./ngram.h"
#include "./search_results.h"
#include "./strategy.h"

namespace codesearch {

class NGramIndexReader {
 public:
  NGramIndexReader(const std::string &index_directory,
                   SearchStrategy strategy);

  NGramIndexReader(const NGramIndexReader &other) = delete;
  NGramIndexReader& operator=(const NGramIndexReader &other) = delete;

  // Find a string in the ngram index. This should be the full query,
  // like "struct foo" or "madvise". If limit is non-zero, it is the
  // max number of results that will be returned.
  void Find(const std::string &query, SearchResults *results);

 private:
  Context *ctx_;
  const std::size_t ngram_size_;
  const IntegerIndexReader files_index_;
  const IntegerIndexReader lines_index_;
  std::vector<SSTableReader<NGram> > shards_;
  SearchStrategy strategy_;

  std::mutex mut_;
  std::condition_variable cond_;
  std::size_t running_threads_;
  const std::size_t parallelism_;

  // Find an ngram smaller than the ngram_size_
  void FindSmall(const std::string &query,
                 SearchResults *results);

  void FindShard(const std::string &query,
                 const std::vector<NGram> &ngrams,
                 const SSTableReader<NGram> &reader,
                 SearchResults *results);

  bool GetCandidates(const NGram &ngram,
                     std::vector<std::uint64_t> *candidates,
                     const SSTableReader<NGram> &reader,
                     SSTableReader<NGram>::iterator *lower_bound);

  // Take the candidates list, and trim the list to only those that
  // are real matches, and add them to the SearchResults object. This
  // returns a guess of how many lines it inserted into the
  // SearchResults object, which will usually be an overestimate
  // (since due to ordering constraints, anything added can be removed
  // later).
  std::size_t TrimCandidates(const std::string &query,
                             const std::string &shard_name,
                             const std::vector<std::uint64_t> &candidates,
                             SearchResults *results);

  // Wait until there are at most target threads running, and then
  // return the number of currently running threads.
  std::size_t WaitForThreads(std::size_t target);

};
}  // codesearch

#endif  // SRC_NGRAM_INDEX_READER_H_
