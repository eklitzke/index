// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_INDEX_READER_H_
#define SRC_INDEX_READER_H_

#include <google/protobuf/message.h>
#include <leveldb/db.h>
#include <string>

#include "./search_results.h"

namespace codesearch {
class IndexReader {
 public:
  explicit IndexReader(const std::string &index_directory)
      :index_directory_(index_directory),
       files_db_(nullptr), ngrams_db_(nullptr), positions_db_(nullptr) {}

  // Verify the contents of the database -- note that Initialize()
  // will call this for you.
  bool Verify();

  // Initialize the database for reading
  bool Initialize();

  // Returns true on success, false on failure.
  bool Search(const std::string &query, SearchResults *results);

  ~IndexReader();

 private:
  const std::string index_directory_;
  std::uint32_t ngram_size_;
  std::uint32_t database_parallelism_;

  leveldb::DB* files_db_;
  leveldb::DB* ngrams_db_;
  leveldb::DB* positions_db_;

  bool GetCandidates(const std::string &ngram,
                     std::vector<std::uint64_t> *candidates);
};
}

#endif  // SRC_INDEX_READER_H_
