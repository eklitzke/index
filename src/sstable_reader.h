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

// This is an implementation of an extremely immutable string class
// taht does *not* copy the string data. Also, for extra speed, some
// bassic assertions about sizes are baked into this. And of course,
// if the thing owning the string data goes away, the FixedString will
// no longer be valid. So you must use this class *very* carefully.
//
// The reason we create this class is because searching the index
// using the C++ iterator API requires that we create lots of
// temporary iterator "value" objects for comparison. Using a
// std::string is fine, but it results in the string doing a new[] and
// delete[] for every temporary string that is created, which causes a
// huge slowdown. In my experiments, compared to the original branch
// that was not using iterators, the slow down in QPS was about 50%
// and sometimes even slower.
//
// Since the index is mmapped for the entire of a search (and the
// strings that get compared to the iterators are already known to be
// appropriately padded), it's safe to not copy string data, which is
// why we can use this technique.
class FixedString {
 public:
  FixedString() = delete;
  FixedString(const char *addr, std::size_t size)
      :addr_(addr), size_(size) {}

  inline bool operator==(const FixedString &other) const {
    return addr_ == other.addr_;
  }

  inline bool operator==(const std::string &other) const {
#ifdef ENABLE_SLOW_ASSERTS
    assert(size_ == other.size());
#endif
    return memcmp(addr_, other.data(), size_) == 0;
  }

  inline bool operator!=(const std::string &other) const {
#ifdef ENABLE_SLOW_ASSERTS
    assert(size_ == other.size());
#endif
    return memcmp(addr_, other.data(), size_) != 0;
  }

  inline bool operator<(const FixedString &other) const {
#ifdef ENABLE_SLOW_ASSERTS
    assert(size_ == other.size_);
#endif
    return memcmp(addr_, other.addr_, size_) < 0;
  }

  inline bool operator<(const std::string &other) const {
#ifdef ENABLE_SLOW_ASSERTS
    assert(size_ == other.size());
#endif
    return memcmp(addr_, other.data(), size_) < 0;
  }

 private:
  const char *addr_;
  std::size_t size_;
};

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
                                    const FixedString,
                                    std::random_access_iterator_tag,
                                    const FixedString> {
   private:
    friend class boost::iterator_core_access;

   public:
    key_iterator() :reader_(nullptr), offset_(0) {}
    key_iterator(const SSTableReader *reader, std::ptrdiff_t off)
        :reader_(reader), offset_(off) {}

    inline const SSTableReader* reader() const { return reader_; }
    inline std::ptrdiff_t offset() const { return offset_; }
    inline std::string value() const {
      return reader_->ReadVal(offset_ * reader_->key_storage());
    }

   private:
    const SSTableReader *reader_;
    std::ptrdiff_t offset_;

    inline const FixedString dereference() const {
      return reader_->ReadKey(offset_ * reader_->key_storage());
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

  const iterator lower_bound(const iterator lower,
                             const iterator upper,
                             const std::string &key) const {
    return std::lower_bound(lower, upper, key);
  }

#if 0
  const iterator upper_bound(const std::string &key) const {
    return std::upper_bound(begin(), end(), key);
  }
#endif

  inline std::uint64_t num_keys() const { return hdr_.num_keys(); }

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

  // pad a search key
  inline void PadNeedle(std::string *needle) const {
    if (needle->size() < key_size_) {
      needle->insert(needle->begin(), key_size_ - needle->size(), '\0');
    }
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

  inline std::size_t key_storage() const {
    return key_size_ + sizeof(std::uint64_t);
  }

  inline FixedString ReadKey(std::ptrdiff_t index_offset) const {
#ifdef ENABLE_SLOW_ASSERTS
    assert(index_offset >= 0 &&
           index_offset < static_cast<std::ptrdiff_t>(hdr_.index_size()));
#endif
    return FixedString(mmap_addr_ + hdr_.index_offset() + index_offset,
                       key_size_);
  }

  inline std::string ReadVal(std::ptrdiff_t index_offset) const {
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
