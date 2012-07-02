// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_SHARD_READER_H_
#define SRC_SHARD_READER_H_

#include "./index.pb.h"
#include "./search_results.h"

#include <google/protobuf/message.h>
#include <leveldb/db.h>
#include <string>

namespace codesearch {
class ShardReader {
 public:

 public:
  ShardReader(const std::string &index_directory,
              std::uint32_t ngram_size,
              std::uint32_t shard_num)
      :index_directory_(index_directory), ngram_size_(ngram_size),
       shard_num_(shard_num), files_db_(nullptr), ngrams_db_(nullptr),
       positions_db_(nullptr) {}

  // Initialize the shard for reading
  bool Initialize();

  // Returns true on success, false on failure.
  bool Search(const std::string &query);

  // Access the search results.
  const SearchResults& results() const { return results_; }

  ~ShardReader();

 private:
  const std::string index_directory_;
  const std::uint32_t ngram_size_;
  const std::uint32_t shard_num_;

  leveldb::DB* files_db_;
  leveldb::DB* ngrams_db_;
  leveldb::DB* positions_db_;

  SearchResults results_;

  bool GetCandidates(const std::string &ngram,
                     std::vector<std::uint64_t> *candidates);
};
}

#endif
