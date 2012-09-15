// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_SSTABLE_READER_H_
#define SRC_SSTABLE_READER_H_

#include "./index.pb.h"
#include "./mmap.h"
#include "./ngram.h"
#include "./util.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>

#include <boost/iterator/iterator_facade.hpp>
#include <snappy.h>

static_assert(sizeof(std::uint64_t) == 8, "Something's whack with uint64_t");

namespace codesearch {
template <typename T>
class SSTableReader {
 public:
  explicit SSTableReader(const std::string &name) :name_(name) {
    std::pair<std::size_t, const char *> mmap_data = GetMmapForFile(name);
    std::size_t mmap_size = mmap_data.first;
    mmap_addr_ = mmap_data.second;
    std::array<std::uint8_t, 8> hdr_size_data;
    memcpy(hdr_size_data.data(), mmap_addr_, sizeof(std::uint64_t));
    std::uint64_t hdr_size = ToUint64(hdr_size_data);
    assert(hdr_size > 0 && hdr_size <= 1024);  // sanity check
    std::unique_ptr<char []> hdr_data(new char[hdr_size]);
    memcpy(hdr_data.get(), mmap_addr_ + sizeof(std::uint64_t), hdr_size);
    std::string hdr_string(hdr_data.get(), hdr_size);
    hdr_.ParseFromString(hdr_string);
    std::string padding = GetWordPadding(hdr_size);

    assert(mmap_size == sizeof(std::uint64_t) + hdr_size + padding.size() +
           hdr_.index_size() + hdr_.data_size());

    // are we using snappy?
    use_snappy_ = hdr_.uses_snappy();

    assert(hdr_.key_size() == key_size);
  }

  // We implement a copy constructor so we can create a std::vector of
  // SSTableReader objects in the NGramIndexReader. No other copying
  // should be happening, however.
  SSTableReader(const SSTableReader<T> &other)
      :name_(other.name_), mmap_addr_(other.mmap_addr_), hdr_(other.hdr_),
       use_snappy_(other.use_snappy_) {}

  SSTableReader& operator=(const SSTableReader &other) = delete;

  enum {
    key_size    = sizeof(std::uint64_t),
    key_storage = key_size + sizeof(std::uint64_t)
  };

  class key_iterator :
      public boost::iterator_facade<key_iterator,
                                    const T,
                                    std::random_access_iterator_tag,
                                    const T> {
   private:
    friend class boost::iterator_core_access;

  public:
    key_iterator() :reader_(nullptr), offset_(0) {}
    key_iterator(const SSTableReader *reader, std::ptrdiff_t off)
        :reader_(reader), offset_(off) {}

    inline const SSTableReader* reader() const { return reader_; }
    inline std::ptrdiff_t offset() const { return offset_; }
    inline std::string value() const {
      return reader_->ReadVal(offset_ * key_storage);
    }

   private:
    const SSTableReader *reader_;
    std::ptrdiff_t offset_;

    inline const T dereference() const {
      return reader_->ReadKey(offset_ * key_storage);
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

  const iterator lower_bound(const T &key) const {
    return std::lower_bound(begin(), end(), key);
  }

  const iterator lower_bound(const iterator lower,
                             const T &key) const {
    return std::lower_bound(lower, end(), key);
  }

  const iterator lower_bound(const iterator lower,
                             const iterator upper,
                             const T &key) const {
    return std::lower_bound(lower, upper, key);
  }

#if 0
  const iterator upper_bound(const T &key) const {
    return std::upper_bound(begin(), end(), key);
  }
#endif

  inline std::uint64_t num_keys() const { return hdr_.num_keys(); }

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

  // buffer to hold snappy data
  mutable std::string snappy_data_;

  // and a mutex to lock access
  mutable std::mutex snappy_data_mut_;

  T ReadKey(std::ptrdiff_t index_offset) const;

  inline std::string ReadVal(std::ptrdiff_t index_offset) const {
    assert(index_offset >= 0 &&
           index_offset < static_cast<std::ptrdiff_t>(hdr_.index_size()));
    std::uint64_t data_offset = ReadUint64(
        mmap_addr_ + hdr_.index_offset() + index_offset + key_size);
    assert(data_offset < UINT32_MAX);  // sanity check
    const char *val_data = mmap_addr_ + hdr_.data_offset() + data_offset;
    std::uint32_t data_size = ReadUint32(val_data);
#ifdef ENABLE_SLOW_ASSERTS
    assert(data_size > 0);
#endif
    std::string data(val_data + sizeof(data_size), data_size);
    if (use_snappy_) {
      std::lock_guard<std::mutex> guard(snappy_data_mut_);
      assert(snappy::Uncompress(data.data(), data.size(), &snappy_data_));
      return snappy_data_;
    } else {
      return data;
    }
  }
};

template<>
inline std::uint64_t SSTableReader<std::uint64_t>::ReadKey(
    std::ptrdiff_t index_offset) const {
#ifdef ENABLE_SLOW_ASSERTS
  assert(index_offset >= 0 &&
         index_offset < static_cast<std::ptrdiff_t>(hdr_.index_size()));
#endif
  return ReadUint64(mmap_addr_ + hdr_.index_offset() + index_offset);
}

#if 0
template<>
inline std::uint32_t SSTableReader<std::uint32_t>::ReadKey(
    std::ptrdiff_t index_offset) const {
#ifdef ENABLE_SLOW_ASSERTS
  assert(index_offset >= 0 &&
         index_offset < static_cast<std::ptrdiff_t>(hdr_.index_size()));
#endif
  return ReadUint32(mmap_addr_ + hdr_.index_offset() + index_offset);
}
#endif

template<>
inline NGram SSTableReader<NGram>::ReadKey(std::ptrdiff_t index_offset) const {
#ifdef ENABLE_SLOW_ASSERTS
  assert(index_offset >= 0 &&
         index_offset < static_cast<std::ptrdiff_t>(hdr_.index_size()));
#endif
  return NGram(mmap_addr_ + hdr_.index_offset() + index_offset +
               key_size - NGram::ngram_size);
}
}  // namespace codesearch
#endif
