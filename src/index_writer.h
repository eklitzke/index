// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_INDEX_WRITER_H_
#define SRC_INDEX_WRITER_H_

#include "./index.pb.h"
#include "./shard_writer.h"

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
       next_shard_(0) {}

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

  std::vector<ShardWriter*> shards_;
  IndexConfig_DatabaseState state_;

  std::size_t next_shard_;

  void WriteStatus(IndexConfig_DatabaseState new_state);
};
}

#endif  // SRC_INDEX_WRITER_H_
