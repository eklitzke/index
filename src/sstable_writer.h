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

enum class WriterState : std::uint8_t {
  UNINITIALIZED = 0,
  INITIALIZED = 1,
  MERGED = 3
};

class SSTableWriter {
 public:
  SSTableWriter(const std::string &name,
                std::size_t key_size,
                bool use_snappy = true);

  // Add a key/value to the database
  void Add(const std::string &key, const std::string &val);
  void Add(const std::uint64_t key, const google::protobuf::Message &val);
  void Add(const std::string &key, const google::protobuf::Message &val);
  void Add(const google::protobuf::Message &key,
           const google::protobuf::Message &val);

  // Merge the index and data files into a single, unified SSTable
  void Merge();

  // Get the current size of the table, as if it were merged
  std::size_t Size() {
    return sizeof(std::uint64_t) * 2 + index_size() + data_size();
  }

 private:
  // The name of the file
  const std::string name_;

  // Whether or not we're compressing values with snappy.
  const bool use_snappy_;

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

  // the numbre of keys in the table
  std::uint64_t num_keys_;

  std::uint64_t index_size() {
    std::streampos pos = idx_out_.tellp();
    assert(pos >= 0);
    return static_cast<std::uint64_t>(pos);
  }

  std::uint64_t data_size() {
    std::streampos pos = data_out_.tellp();
    assert(pos >= 0);
    return static_cast<std::uint64_t>(pos);
  }

  // Write a value out to the data part of the index
  void AddValue(const std::string &val);

  void WriteMergeContents(std::ifstream *is, std::ofstream *os);
};

}  // namespace codesearch
#endif  // SRC_SSTABLE_WRITER_H_
