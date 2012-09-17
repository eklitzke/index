// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_INDEX_WRITER_H_
#define SRC_INDEX_WRITER_H_

#include "./index.pb.h"
#include "./sstable_writer.h"

#include <google/protobuf/message.h>
#include <string>
#include <vector>

namespace codesearch {

class IndexWriter {
 public:
  // Create an index writer. This is an object that can be used to
  // write out an index.
  //
  // Args:
  //  index_directory: string, the directory containing the index
  //  name: string, the name of the index
  //  key_size: size_t, the max size of a key in the index
  //  shard_size: size_t, the max size in bytes of each shard
  IndexWriter(const std::string &index_directory,
              const std::string &name,
              std::size_t key_size,
              std::size_t shard_size,
              bool auto_rotate = true);

  void SetKeyType(IndexConfig_KeyType key_type) {
    key_type_ = key_type;
  }

  template <typename U, typename V>
  void Add(const U &key, const V &value) {
    EnsureSSTable();
    sstable_->Add(key, value);
    if (auto_rotate_ && sstable_->Size() >= shard_size_) {
      Rotate();
    }
  }

  // Rotate the current SSTable. Should only be called outside of this
  // class' implementation when auto_rotate is set to false.
  void Rotate();

  std::size_t shard_size() const { return shard_size_; }

  ~IndexWriter();

 private:
  const std::string index_directory_;
  const std::string name_;
  const std::size_t key_size_;
  const std::size_t shard_size_;
  const bool auto_rotate_;
  std::size_t shard_num_;
  IndexConfig_KeyType key_type_;
  IndexConfig_DatabaseState state_;
  SSTableWriter *sstable_;

  // Get a full filename that resides within the index directory.
  std::string GetPathName(const std::string &filename);

  // Ensure that there is an SSTable present (which is not the case
  // immediately after creating an IndexWriter, or immediately after a
  // rotation -- this is to present the creation of empty SSTables.)
  void EnsureSSTable();

  void WriteStatus(IndexConfig_DatabaseState new_state);
};
}  // namespace codesearch

#endif  // SRC_INDEX_WRITER_H_
