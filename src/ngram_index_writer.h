// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_NGRAM_INDEX_WRTITER_H_
#define SRC_NGRAM_INDEX_WRTITER_H_

#include "./index_writer.h"
#include "./integer_index_writer.h"
#include "./ngram.h"

#include <condition_variable>
#include <mutex>
#include <string>

namespace codesearch {
class NGramIndexWriter {
 public:
  NGramIndexWriter(const std::string &index_directory,
                   std::size_t ngram_size = 3,
                   std::size_t shard_size = 16 << 20,
                   std::size_t max_threads = 1);

  void AddFile(const std::string &canonical_name,
               const std::string &dir_name,
               const std::string &file_name);

  ~NGramIndexWriter();

 private:
  IndexWriter index_writer_;
  IntegerIndexWriter files_index_;
  IntegerIndexWriter lines_index_;

  std::map<NGram, std::vector<std::uint64_t> > lists_;
  const std::size_t ngram_size_;
  std::size_t num_vals_;

  const std::string index_directory_;
  const std::size_t max_threads_;
  std::size_t threads_running_;
  std::condition_variable cond_;
  std::mutex threads_running_mut_;
  std::mutex ngrams_mut_;

  // a map of file id to starting line in the file
  FileStartLines file_start_lines_;

  void AddFileThread(const std::string &canonical_name,
                     const std::string &dir_name,
                     const std::string &file_name);

  void Add(const NGram &ngram, const std::vector<std::uint64_t> &vals);

  // Estimate the size of the index that will be written
  std::size_t EstimateSize();

  // Rotate the index writer, if the index is large enough
  void MaybeRotate(bool force = false);

  // Used to notify the condition variable
  void Notify();

  // Wait for all threads to finish
  void Wait();
};
}

#endif
