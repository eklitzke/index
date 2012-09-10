// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_SSTABLE_READER_H_
#define SRC_SSTABLE_READER_H_

#include "./index.pb.h"
#include "./ngram.h"
#include "./util.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <string>

#include <boost/iterator/iterator_facade.hpp>
#include <snappy.h>

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
       use_snappy_(other.use_snappy_), key_size_(other.key_size_) {}

  SSTableReader& operator=(const SSTableReader &other) = delete;

  typedef std::pair<std::string, std::string> value_type;

  class key_iterator :
      public boost::iterator_facade<key_iterator,
                                    const std::string,
                                    std::random_access_iterator_tag,
                                    const std::string> {
   private:
    friend class boost::iterator_core_access;

   public:
    key_iterator() = delete;
    key_iterator(const SSTableReader *reader, std::ptrdiff_t off)
        :reader_(reader), offset_(off) {}

    inline std::ptrdiff_t offset() const { return offset_; }
    inline std::string value() const {
      return reader_->read_val(offset_ * reader_->key_storage());
    }

   private:
    const SSTableReader *reader_;
    std::ptrdiff_t offset_;

    inline std::string dereference() const {
      return reader_->read_key(offset_ * reader_->key_storage());
    }

    inline void increment() { offset_++; }
    inline void decrement() { offset_--; }
    inline void advance(std::ptrdiff_t n) { offset_ += n; }

    inline std::ptrdiff_t distance_to(const key_iterator &other) const {
#ifdef ENABLE_SLOW_ASSERTS
      assert(reader_->mmap_addr_ == other.reader_->mmap_addr_);
#endif
      return other.offset_ - offset_;
    }

    inline bool equal(const key_iterator &other) const {
      return offset_ == other.offset_ &&
          reader_->mmap_addr_ == other.reader_->mmap_addr_;
    }
  };

  typedef key_iterator iterator;

  const iterator begin() const { return iterator(this, 0); }
  const iterator end() const { return iterator(this, hdr_.num_keys()); }

  const iterator lower_bound(const std::string &key) const {
    return std::lower_bound(begin(), end(), key);
  }

  const iterator lower_bound(const iterator lower,
                             const std::string &key) const {
    return std::lower_bound(lower, end(), key);
  }

  const iterator upper_bound(const std::string &key) const {
    return std::upper_bound(begin(), end(), key);
  }

  inline bool Find(std::uint64_t needle, std::string *result) const {
    std::string val = Uint64ToString(needle);
    PadNeedle(&val);
    return InnerFind(val, result);
  }

  inline bool Find(std::string needle, std::string *result) const {
    PadNeedle(&needle);
    return InnerFind(needle, result);
  }

  inline bool Find(const NGram &ngram, std::string *result) const {
    return InnerFind(ngram.padded_string(), result);
  }

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

  // pad a search key
  inline void PadNeedle(std::string *needle) const {
    std::uint64_t key_size = hdr_.key_size();
    if (needle->size() < key_size) {
      needle->insert(needle->begin(), key_size - needle->size(), '\0');
   }
  }

  inline bool InnerFind(const std::string &needle,
                        std::string *result_string) const {
    iterator pos = lower_bound(needle);
    if (*pos == needle) {
      *result_string = pos.value();
      return true;
    }
    return false;
  }

  inline std::size_t key_storage() const {
    return key_size_ + sizeof(std::uint64_t);
  }

  inline std::string read_key(std::ptrdiff_t index_offset) const {
#ifdef ENABLE_SLOW_ASSERTS
    assert(index_offset >= 0 &&
           index_offset < static_cast<std::ptrdiff_t>(hdr_.index_size()));
#endif
    std::string key(
        mmap_addr_ + hdr_.index_offset() + index_offset, key_size_);
    return key;

  }

  inline std::string read_val(std::ptrdiff_t index_offset) const {
    assert(index_offset >= 0 &&
           index_offset < static_cast<std::ptrdiff_t>(hdr_.index_size()));
    std::uint64_t data_offset = ReadUint64(
        mmap_addr_ + hdr_.index_offset() + index_offset + key_size_);
    assert(data_offset < UINT32_MAX);  // sanity check
    const char *val_data = mmap_addr_ + hdr_.data_offset() + data_offset;
    std::uint32_t data_size = ReadUint32(val_data);
    std::string data(val_data + sizeof(data_size), data_size);
    if (use_snappy_) {
      std::string uncompressed_data;
      assert(snappy::Uncompress(data.data(), data.size(), &uncompressed_data));
      return uncompressed_data;
    } else {
      return data;
    }
  }



};
}

#endif
