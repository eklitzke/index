// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_AUTOINCREMENT_INDEX_H_
#define SRC_AUTOINCREMENT_INDEX_H_

#include "./index_writer.h"
#include <string>

namespace codesearch {
class AutoIncrementIndexWriter {
 public:
  AutoIncrementIndexWriter(const std::string &index_directory,
                           const std::string &name,
                           std::size_t shard_size = 16 << 20)
      :index_writer_(index_directory, name, sizeof(std::uint64_t), shard_size),
       val_(0) {
    index_writer_.Initialize();
  }

  // Add a value to the index using an autoincrement value, and
  // return the value for the key that was added.
  template<typename T>
  std::uint64_t Add(const T &val) {
    index_writer_.Add(val_, val);
    return val_++;
  }

 private:
    IndexWriter index_writer_;
    std::uint64_t val_;
};
}

#endif  // SRC_AUTOINCREMENT_INDEX_H_
