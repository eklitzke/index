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

  // When searching for multiple needles (e.g. as in a SQL "IN"
  // query), an optimization that can be done is to search for the
  // terms in sorted order, and for each successive term save the
  // lower bound from the previous term. This optimization is applied
  // in index_reader.cc, and is the motivation for the FindWithBounds
  // methods.
  bool FindWithBounds(const char *needle, std::string *result,
                      std::size_t *lower_bound);
  bool FindWithBounds(const std::string &needle, std::string *result,
                      std::size_t *lower_bound);

  std::size_t lower_bound() const { return 0; }
  std::size_t upper_bound() const { return index_len_ >> 4; }

  ~SSTableReader();

 private:
  std::string name_;
  FILE* index_file_;
  const char *mmap_addr_;

  std::size_t data_len_;
  std::size_t index_len_;

  void PadString(const std::string &in, std::string *out);
};
}

#endif
