// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_NGRAM_INDEX_WRTITER_H_
#define SRC_NGRAM_INDEX_WRTITER_H_

#include "./index_writer.h"
#include "./integer_index_writer.h"
#include "./ngram.h"
#include "./ngram_counter.h"
#include "./thread_util.h"

#include <memory>
#include <string>

namespace codesearch {
class NGramIndexWriter {
 public:
  NGramIndexWriter(const std::string &index_directory,
                   std::size_t ngram_size = 3,
                   std::size_t shard_size = 16 << 20,
                   std::size_t max_threads = 1);

  void AddDocument(std::uint64_t document_id,
                   std::istream *input,
                   bool delete_input = false);

  ~NGramIndexWriter();

 private:
  const std::string index_directory_;

  IndexWriter index_writer_;
  IntegerIndexWriter lines_index_;

  std::map<NGram, std::vector<std::uint64_t> > lists_;
  const std::size_t ngram_size_;
  std::size_t file_count_;
  std::size_t num_vals_;

  IntWait positions_wait_;
  IntWait ngrams_wait_;

  FunctionThreadPool pool_;

  // a map of file id to starting line in the file
  DocumentStartLines document_start_lines_;

  NGramCounter counter_;

  void AddDocumentThread(std::uint64_t document_id,
                         std::istream *input,
                         bool delete_input);

  void Add(const NGram &ngram, const std::vector<std::uint64_t> &vals);

  // Estimate the size of the index that will be written
  std::size_t EstimateSize();

  // Rotate the index writer, if the index is large enough
  void MaybeRotate(bool force = false);
};
}

#endif
