// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_NGRAM_INDEX_READER_H_
#define SRC_NGRAM_INDEX_READER_H_

#include <string>
#include <vector>

#include "./bounded_map.h"
#include "./context.h"
#include "./integer_index_reader.h"
#include "./ngram.h"
#include "./queue.h"
#include "./search_results.h"
#include "./strategy.h"

namespace codesearch {

class NGramReaderWorker;

struct QueryRequest {
  QueryRequest(const std::string &q,
               const std::vector<NGram> &n,
               SearchResults *r)
      :query(q), ngrams(n), results(r) {}

  const std::string &query;
  const std::vector<NGram> &ngrams;
  SearchResults *results;
};


class NGramIndexReader {
 public:
  NGramIndexReader(const std::string &index_directory,
                   SearchStrategy strategy,
                   std::size_t threads = 0);
  ~NGramIndexReader();

  NGramIndexReader(const NGramIndexReader &other) = delete;
  NGramIndexReader& operator=(const NGramIndexReader &other) = delete;

  // Find a string in the ngram index. This should be the full query,
  // like "struct foo" or "madvise". If limit is non-zero, it is the
  // max number of results that will be returned.
  void Find(const std::string &query, SearchResults *results);

 private:
  friend class NGramReaderWorker;

  Context *ctx_;
  const IntegerIndexReader files_index_;
  const IntegerIndexReader lines_index_;
  std::vector<SSTableReader<NGram> > shards_;
  SearchStrategy strategy_;

  const std::size_t parallelism_;

  Queue<NGramReaderWorker*> response_queue_;
  Queue<NGramReaderWorker*> terminate_response_queue_;
  std::vector<NGramReaderWorker*> free_workers_;

  // Find an ngram smaller than the ngram_size_
  void FindSmall(const std::string &query,
                 SearchResults *results);

  // Find according to a list of ngrams. This is the thing that looks
  // up each ngram, and then intersects the results from each ngram query.
  void FindNGrams(const std::string &query,
                  const std::vector<NGram> ngrams,
                  SearchResults *results);
};

}  // codesearch

#endif  // SRC_NGRAM_INDEX_READER_H_
