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
  SSTableWriter(const std::string &name)
      :name_(name), state_(UNINITIALIZED), last_key_(8, '\0'), offset_(0) {}

  void Initialize();
  void Add(const std::string &key, const std::string &val);
  void Add(const std::uint64_t key, const google::protobuf::Message &val);
  void Add(const std::string &key, const google::protobuf::Message &val);
  void Add(const google::protobuf::Message &key,
           const google::protobuf::Message &val);
  void Merge();
 private:
  std::string name_;
  WriterState state_;
  std::ofstream idx_out_;
  std::ofstream data_out_;
  std::string last_key_;
  std::uint64_t offset_;


  void WriteMergeSize(std::ifstream *is, std::ofstream *os);
  void WriteMergeContents(std::ifstream *is, std::ofstream *os);
};

}  // namespace codesearch
#endif  // SRC_SSTABLE_WRITER_H_
