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

class QueryRequest;
class NGramReaderWorker;

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


class NGramReaderWorker {
 public:
  NGramReaderWorker(Queue<NGramReaderWorker*> *responses,
                    Queue<NGramReaderWorker*> *terminate_responses,
                    NGramIndexReader *index_reader);

  void Run();  // run in a loop
  void Stop();  // stop running
  void SendRequest(const QueryRequest *req, const SSTableReader<NGram> *shard);

 private:
  Queue<NGramReaderWorker*> *responses_;
  Queue<NGramReaderWorker*> *terminate_responses_;
  const NGramIndexReader *index_reader_;
  bool keep_going_;

  const QueryRequest* req_;
  const SSTableReader<NGram> *shard_;

  std::mutex mut_;
  std::condition_variable cond_;

  // The wrapper function that coordinates the logic for searching a
  // single SSTableReader<NGram> (i.e. a single SSTable file).
  void FindShard();

  // Given an ngram, a result list, an "ngram" reader shard, and a
  // lower bound, fill in the result list with all of the positions in
  // the posting list for that ngram. This is the function that
  // actually does the posting-list lookup in the "ngrams" SSTable.
  bool GetCandidates(const NGram &ngram,
                     std::vector<std::uint64_t> *candidates,
                     SSTableReader<NGram>::iterator *lower_bound,
                     NGramValue *ngram_Val);


  // Given a vector of candidate position ids, this function actually
  // looks up the position data to see if the positions are true
  // matches, and fills in the SearchResults object. This method does
  // quries agains the "lines" and "files" SSTables.
  std::size_t TrimCandidates(const std::vector<std::uint64_t> &candidates);
};

}  // codesearch

#endif  // SRC_NGRAM_INDEX_READER_H_
