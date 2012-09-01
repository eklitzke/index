// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_SSTABLE_READER_H_
#define SRC_SSTABLE_READER_H_

#include "./index.pb.h"
#include "./ngram.h"

#include <cstring>
#include <string>

static_assert(sizeof(std::uint64_t) == 8, "Something's whack with uint64_t");

namespace codesearch {
class SSTableReader {
 public:
  explicit SSTableReader(const std::string &name);

  // We implement a copy constructor so we can create a std::vector of
  // SSTableReader objects in the NGramIndexReader. No other copying
  // should be happening, however.
  SSTableReader(const SSTableReader &other)
      :name_(other.name_), mmap_addr_(other.mmap_addr_), hdr_(other.hdr_),
       key_size_(other.key_size_), pad_(nullptr) {
    pad_ = new char[key_size_];
    memset(pad_, 0, key_size_);
  }

  SSTableReader& operator=(const SSTableReader &other) = delete;

  ~SSTableReader() { delete[] pad_; }

  // Checks that a given needle is within the min/max bounds for this table.
  bool CheckMinMaxBounds(const char *needle, std::size_t *lower_bound) const;

  bool Find(std::uint64_t needle, std::string *result) const;
  bool Find(std::string needle, std::string *result) const;
  bool Find(const NGram &ngram, std::string *result) const;

  // When searching for multiple needles (e.g. as in a SQL "IN"
  // query), an optimization that can be done is to search for the
  // terms in sorted order, and for each successive term save the
  // lower bound from the previous term. This optimization is applied
  // in index_reader.cc, and is the motivation for the FindWithBounds
  // methods.
  bool FindWithBounds(const char *needle, std::size_t needle_size,
                      std::string *result, std::size_t *lower_bound) const;

  inline std::size_t key_size() const { return key_size_; }

  inline std::string min_key() const {
    std::string retval = hdr_.min_value();
    PadNeedle(&retval);
    return retval;
  }

  inline std::string max_key() const {
    std::string retval = hdr_.max_value();
    PadNeedle(&retval);
    return retval;
  }

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

  // whether or not we're using snappy
  bool use_snappy_;

  // the key size
  std::size_t key_size_;

  // the pad; concurrent access to this makes this class not thread-safe
  char *pad_;

  // pad a search key
  void PadNeedle(std::string *needle) const;

  // The internal version of FindWithBounds; since the needle has to
  // be properly padded, this version is made private (i.e. purely to
  // ensure that proper padding has been done).
  bool FindWithBounds(const char *needle, std::string *result,
                      std::size_t *lower_bound) const;

  inline std::size_t lower_bound() const { return 0; }
  inline std::size_t upper_bound() const { return hdr_.num_keys(); }

};
}

#endif
