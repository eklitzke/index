// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_INTEGER_INDEX_WRITER_H_
#define SRC_INTEGER_INDEX_WRITER_H_

#include <mutex>
#include <string>

#include "./index_writer.h"

namespace codesearch {
class IntegerIndexWriter {
 public:
  IntegerIndexWriter(const std::string &index_directory,
                     const std::string &name,
                     std::size_t shard_size = UINT32_MAX)
      :index_writer_(index_directory, name, sizeof(std::uint64_t), shard_size),
       next_key_(0) {}

  // Add a value to the index using an autoincrement value, and
  // return the value for the key that was added.
  template<typename T>
  std::uint64_t Add(const T &val) {
    std::lock_guard<std::mutex> guard(mut_);
    index_writer_.Add(next_key_, val);
    return next_key_++;
  }

 private:
  IndexWriter index_writer_;
  std::uint64_t next_key_;
  std::mutex mut_;
};
}

#endif  // SRC_INTEGER_INDEX_WRITER_H_
