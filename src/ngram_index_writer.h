// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_NGRAM_INDEX_WRTITER_H_
#define SRC_NGRAM_INDEX_WRTITER_H_

#include "./autoincrement_index_writer.h"
#include "./index_writer.h"
#include <string>

namespace codesearch {
class NGramIndexWriter {
 public:
  NGramIndexWriter(const std::string &index_directory,
                   std::size_t ngram_size = 3,
                   std::size_t shard_size = 16 << 20);

  void AddFile(const std::string &canonical_name,
               const std::string &dir_name,
               const std::string &file_name);

  ~NGramIndexWriter();

 private:
  IndexWriter index_writer_;
  AutoIncrementIndexWriter files_index_;
  AutoIncrementIndexWriter lines_index_;

  std::map<std::string, std::vector<std::uint64_t> > lists_;
  const std::size_t ngram_size_;
  std::size_t num_vals_;

  void Add(const std::string &ngram, const std::vector<std::uint64_t> &vals);

  // Estimate the size of the index that will be written
  std::size_t EstimateSize();

  // Rotate the index writer, if the index is large enough
  void MaybeRotate(bool force = false);
};
}

#endif
