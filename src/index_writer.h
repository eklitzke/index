// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_INDEX_WRITER_H_
#define SRC_INDEX_WRITER_H_

#include "./autoincrement.h"
#include "./index.pb.h"
#include "./posting_list.h"

#include <google/protobuf/message.h>
#include <leveldb/db.h>
#include <string>

namespace codesearch {

class IndexWriter {
 public:
  IndexWriter(const std::string &index_directory,
              std::uint32_t ngram_size = 3,
              std::uint32_t database_parallelism = 8,
              std::uint32_t create_threads = 8)
      :index_directory_(index_directory), ngram_size_(ngram_size),
       database_parallelism_(database_parallelism),
       create_threads_(create_threads), state_(IndexConfig_DatabaseState_EMPTY),
       files_db_(nullptr), ngrams_db_(nullptr), positions_db_(nullptr) {}

  // Initialize the Indexer object. Returns true on success, false on
  // failure.
  bool Initialize();

  void AddFile(const std::string &filename);

  ~IndexWriter();

 private:
  const std::string index_directory_;
  const std::uint32_t ngram_size_;
  const std::uint32_t database_parallelism_;
  const std::uint32_t create_threads_;
  IndexConfig_DatabaseState state_;

  PostingList ngrams_;

  // Data for the "files" database
  AutoIncrement<std::uint64_t> files_id_;
  leveldb::DB* files_db_;

  // Data for the "ngrams" database
  leveldb::DB* ngrams_db_;

  // Data for the "positions" database
  AutoIncrement<std::uint64_t> positions_id_;
  leveldb::DB* positions_db_;

  void WriteStatus(IndexConfig_DatabaseState new_state);

  void FinalizeDb(leveldb::DB* db);
};
}

#endif  // SRC_INDEX_WRITER_H_
