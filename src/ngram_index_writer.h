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
// This is a helper class for waiting for an integer to reach some
// value, and then acquiring a lock; releasing the lock increments the
// internal counter. This is used to help enforce an ordering of how
// files/positions/ngrams are written out in the index writer.
class IntWait {
 public:
  explicit IntWait(std::size_t start = 0) :val_(start) {}
  IntWait(const IntWait &other) = delete;
  IntWait& operator=(const IntWait &other) = delete;

  void Acquire(std::size_t val);
  void Release(std::size_t val);
 private:
  std::mutex mut_;
  std::size_t val_;
  std::condition_variable cond_;
};

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
  std::size_t file_count_;
  std::size_t num_vals_;

  const std::string index_directory_;
  const std::size_t max_threads_;
  std::size_t threads_running_;
  std::condition_variable cond_;

  IntWait files_wait_;
  IntWait positions_wait_;
  IntWait ngrams_wait_;

  std::mutex threads_running_mut_;

  // a map of file id to starting line in the file
  FileStartLines file_start_lines_;

  void AddFileThread(std::size_t file_count,
                     const std::string &canonical_name,
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
