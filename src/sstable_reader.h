// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_SSTABLE_READER_H_
#define SRC_SSTABLE_READER_H_

#include <string>


static_assert(sizeof(std::uint64_t) == 8, "Something's whack with uint64_t");

namespace codesearch {
class SSTableReader {
 public:
  explicit SSTableReader(const std::string &name)
      :name_(name), mmap_addr_(nullptr) {}

  void Initialize();

  bool Find(const char *needle, std::string *result);
  bool Find(std::uint64_t needle, std::string *result);
  bool Find(const std::string &needle, std::string *result);

  ~SSTableReader();

 private:
  std::string name_;
  FILE* index_file_;
  const char *mmap_addr_;

  std::size_t index_len_;
  std::size_t data_len_;
  std::size_t index_entries_;
};
}

#endif
