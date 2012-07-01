// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_INDEX_READER_H_
#define SRC_INDEX_READER_H_

#include <google/protobuf/message.h>
#include <leveldb/db.h>
#include <string>

namespace cs {
namespace index {

struct SearchResult {
  std::string filename;
  std::size_t line_num;
  std::string line_text;
};

class IndexReader {
 public:
  IndexReader(const std::string &index_directory, std::size_t ngram_size)
      :index_directory_(index_directory), ngram_size_(ngram_size),
       files_db_(nullptr), ngrams_db_(nullptr), positions_db_(nullptr) {}

  bool Initialize();

  std::vector<SearchResult> Search(const std::string &query);

  ~IndexReader();

 private:
  const std::string index_directory_;
  const std::size_t ngram_size_;

  leveldb::DB* files_db_;
  leveldb::DB* ngrams_db_;
  leveldb::DB* positions_db_;
};
}
}

#endif  // SRC_INDEX_READER_H_
