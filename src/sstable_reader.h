// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_SSTABLE_READER_H_
#define SRC_SSTABLE_READER_H_

#include "./index.pb.h"

#include <string>

static_assert(sizeof(std::uint64_t) == 8, "Something's whack with uint64_t");

namespace codesearch {
class SSTableReader {
 public:
  explicit SSTableReader(const std::string &name);

  bool Find(const char *needle, std::string *result) const;
  bool Find(std::uint64_t needle, std::string *result) const;
  bool Find(const std::string &needle, std::string *result) const;

  // When searching for multiple needles (e.g. as in a SQL "IN"
  // query), an optimization that can be done is to search for the
  // terms in sorted order, and for each successive term save the
  // lower bound from the previous term. This optimization is applied
  // in index_reader.cc, and is the motivation for the FindWithBounds
  // methods.
  bool FindWithBounds(const char *needle, std::string *result,
                      std::size_t *lower_bound) const;
  bool FindWithBounds(const std::string &needle, std::string *result,
                      std::size_t *lower_bound) const;

  std::size_t lower_bound() const { return 0; }
  std::size_t upper_bound() const { return hdr_.num_keys(); }

  std::string shard_name() const {
    std::string::size_type pos = name_.find_last_of('/');
    if (pos == std::string::npos || pos == name_.size() - 1) {
      return name_;
    }
    return name_.substr(pos + 1, std::string::npos);
  }

 private:
  // the name of the backing file
  const std::string name_;

  // this address points to the start of the mmaped index
  const char *mmap_addr_;

  // the header read from the index
  SSTableHeader hdr_;

  // pad a search key
  void PadNeedle(const std::string &in, std::string *out) const;
};
}

#endif
