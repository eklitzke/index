// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_SSTABLE_WRITER_H_
#define SRC_SSTABLE_WRITER_H_

#include <iostream>
#include <fstream>
#include <boost/filesystem.hpp>
#include <google/protobuf/message.h>

static_assert(sizeof(std::uint64_t) == 8, "Something's whack with uint64_t");

namespace codesearch {

enum WriterState {
  UNINITIALIZED = 0,
  INITIALIZED = 1,
  MERGED = 3
};

class SSTableWriter {
 public:
  SSTableWriter(const std::string &name, std::size_t key_size);
  void Initialize();

  // Add a key/value to the database
  void Add(const std::string &key, const std::string &val);
  void Add(const std::uint64_t key, const google::protobuf::Message &val);
  void Add(const std::string &key, const google::protobuf::Message &val);
  void Add(const google::protobuf::Message &key,
           const google::protobuf::Message &val);

  // Merge the index and data files into a single, unified SSTable
  void Merge();

  // Get the current size of the table, as if it were merged
  std::size_t Size() const {
    return sizeof(std::uint64_t) * 2 + index_size_ + data_size_;
  }

 private:
  // The name of the file
  const std::string name_;

  // The size of a key in the index. The index is a sequence of
  // (key_size_, sizeof(std::uint64_t)) pairs.
  std::size_t key_size_;

  // The state of the index writer.
  WriterState state_;

  // The index file
  std::ofstream idx_out_;

  // The data file
  std::ofstream data_out_;

  // The minimum key in the file
  std::string min_key_;

  // The last key seen; this is maintained in order to ensure that the
  // user doesn't accidentally write keys out of order.
  std::string last_key_;

  // number of bytes that have been written to the index
  std::uint64_t index_size_;

  // number of bytes that have been written to the data section
  std::uint64_t data_size_;

  // the numbre of keys in the table
  std::uint64_t num_keys_;

  std::uint64_t FileSize(std::ifstream *is, std::ofstream *os);
  void WriteMergeContents(std::ifstream *is, std::ofstream *os);
};

}  // namespace codesearch
#endif  // SRC_SSTABLE_WRITER_H_
