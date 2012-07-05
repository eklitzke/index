// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_INDEX_READER_H_
#define SRC_INDEX_READER_H_

#include <string>

#include "./search_results.h"
#include "./shard_reader.h"

namespace codesearch {
class IndexReader {
 public:
  explicit IndexReader(const std::string &index_directory)
      :index_directory_(index_directory) {}

  // Verify the contents of the index -- note that Initialize()
  // will call this for you.
  bool Verify();

  // Initialize the index for reading
  bool Initialize();

  // Returns true on success, false on failure.
  bool Search(const std::string &query, SearchResults *results);

  ~IndexReader();

 private:
  const std::string index_directory_;
  std::uint32_t ngram_size_;
  std::uint32_t database_parallelism_;
  std::vector<ShardReader*> shards_;
};
}

#endif  // SRC_INDEX_READER_H_
