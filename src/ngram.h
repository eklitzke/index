// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>
//
// The implementation of an ngram. Note that while the term "ngram"
// can generally refer to, uh, "grams" of any size N, we actually
// implement a hard-coded trigram here. Very naughty. The alternatives
// are to use a class templated on ngram size (which would make *all*
// of the code *everywhere* turn into a giant, templated,
// "headers-only" nightmare), or to use a dynamic ngram size in which
// case we might as well just use a std::string. Plus, in practice
// trigrams are clearly the best ngram size anyway.
//
// We do some tricks to internally store an ngram as a std::uint32_t
// to make some things simpler.

#ifndef SRC_NGRAM_H_
#define SRC_NGRAM_H_

#include <cassert>
#include <cstring>
#include <string>

#include <endian.h>

#include "./util.h"

static_assert(sizeof(std::uint32_t) == 4, "this check is really pointless");

namespace codesearch {

class NGram {
 public:

  const static std::size_t ngram_size = 3;

  NGram() = delete;

  explicit NGram(const char *ngram) {
    // can't strlen the char*, since null bytes are valid
    memcpy(data_.buf, ngram, ngram_size);
    data_.buf[ngram_size] = '\0';
  }


  explicit NGram(const std::string &ngram) {
    assert(ngram.size() == ngram_size);
    memcpy(data_.buf, ngram.data(), ngram_size);
    data_.buf[ngram_size] = '\0';
  }

  NGram(const NGram &other) {
    data_.num = other.data_.num;
  }

  inline NGram& operator=(const NGram &other) {
    data_.num = other.data_.num;
    return *this;
  }

  // despite rule of three, default dtor is OK

  inline bool operator<(const NGram &other) const {
    return num() < other.num();
  }

  inline bool operator<(const char *other) const {
    return memcmp(data_.buf, other, ngram_size) < 0;
  }

  inline bool operator<(const std::string &other) const {
    return memcmp(data_.buf, other.data(), ngram_size) < 0;
  }

  inline bool operator<=(const NGram &other) const {
    return num() <= other.num();
  }

  inline bool operator<=(const char *other) const {
    return memcmp(data_.buf, other, ngram_size) <= 0;
  }

  inline bool operator<=(const std::string &other) const {
    return memcmp(data_.buf, other.data(), ngram_size) <= 0;
  }

  inline bool operator==(const NGram &other) const {
    return data_.num == other.data_.num;
  }

  inline bool operator==(const char *other) const {
    return memcmp(data_.buf, other, ngram_size) == 0;
  }

  inline bool operator==(const std::string &other) const {
    return memcmp(data_.buf, other.data(), ngram_size) == 0;
  }

  inline bool operator!=(const NGram &other) const {
    return data_.num != other.data_.num;
  }

  inline bool operator!=(const char *other) const {
    return memcmp(data_.buf, other, ngram_size) != 0;
  }

  inline bool operator!=(const std::string &other) const {
    return memcmp(data_.buf, other.data(), ngram_size) != 0;
  }

  inline bool operator>=(const NGram &other) const {
    return num() >= other.num();
  }

  inline bool operator>=(const char *other) const {
    return memcmp(data_.buf, other, ngram_size) >= 0;
  }

  inline bool operator>=(const std::string &other) const {
    return memcmp(data_.buf, other.data(), ngram_size) >= 0;
  }

  inline bool operator>(const NGram &other) const {
    return num() > other.num();
  }

  inline bool operator>(const char *other) const {
    return memcmp(data_.buf, other, ngram_size) > 0;
  }

  inline bool operator>(const std::string &other) const {
    return memcmp(data_.buf, other.data(), ngram_size) > 0;
  }

  // Accessors
  inline const char* data() const { return data_.buf; }
  inline const char* c_str() const { return data_.buf; }
  inline std::string string() const {
    return std::string(data_.buf, ngram_size);
  }
  inline std::string padded_string(std::size_t size = 8) const {
    return std::string(size - ngram_size, '\0') +
        std::string(data_.buf, ngram_size);
  }
  inline std::uint32_t num() const { return be32toh(data_.num); }

 private:
  union {
    char buf[sizeof(std::uint32_t)];
    std::uint32_t num;
  } data_;

  static_assert(sizeof(data_) == sizeof(std::uint32_t), "just in case");
};

static_assert(sizeof(NGram) == sizeof(std::uint32_t),
              "compiler generating bad NGram code, naughty");
}  // namespace codesearch

namespace std {
inline ostream& operator<<(ostream &os, const codesearch::NGram &ngram) {
  os << codesearch::PrintBinaryString(ngram.string());
  return os;
}
}  // namespace std

#endif  // SRC_NGRAM_H_
