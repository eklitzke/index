// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_SHARD_WRITER_H_
#define SRC_SHARD_WRITER_H_

#include "./autoincrement.h"
#include "./posting_list.h"
#include "./sstable_writer.h"

#include <string>

namespace codesearch {

class ShardWriter {
 public:
  ShardWriter(const std::string &index_directory,
              std::uint32_t ngram_size,
              std::uint32_t shard_num)
  :index_directory_(index_directory), ngram_size_(ngram_size),
   shard_num_(shard_num), files_db_(nullptr), ngrams_db_(nullptr),
   positions_db_(nullptr) {}

  bool Initialize();

  void AddFile(const std::string &filename);

  ~ShardWriter();

 private:
  const std::string index_directory_;
  const std::uint32_t ngram_size_;
  const std::uint32_t shard_num_;

  PostingList ngrams_;

  // Data for the "files" database
  AutoIncrement<std::uint64_t> files_id_;
  SSTableWriter *files_db_;

  // Data for the "ngrams" database
  SSTableWriter *ngrams_db_;

  // Data for the "positions" database
  AutoIncrement<std::uint64_t> positions_id_;
  SSTableWriter *positions_db_;

  std::string DatabasePath(const std::string &name);
  void FinalizeDb(SSTableWriter* db);
};
}  // namespace index
#endif
