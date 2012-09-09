// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./sstable_reader.h"
#include "./util.h"
#include "./mmap.h"

#include <glog/logging.h>

#include <array>
#include <cassert>
#include <cstring>
#include <memory>
#include <iostream>

#include <snappy.h>
#include <sys/mman.h>

namespace codesearch {
SSTableReader::SSTableReader(const std::string &name)
    :name_(name), pad_(nullptr) {
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

  key_size_ = hdr_.key_size();
  pad_ = new char[key_size_];
  memset(pad_, 0, key_size_);
}

bool SSTableReader::FindWithBounds(const char *needle,
                                   std::size_t needle_size,
                                   std::string *result,
                                   std::size_t *lower_bound) const {
  const std::size_t key_size = hdr_.key_size();

  // We don't zero pad here, because pad_ is zero filled when it's
  // allocated, and needles should always be the same size (since
  // ngrams are always the same size). If that assumption changes,
  // breakage will occur here, and we'll need to memset with zeroes.
  memcpy(pad_ + key_size - needle_size, needle, needle_size);
  return FindWithBounds(pad_, result, lower_bound);
}

bool SSTableReader::Find(std::uint64_t needle, std::string *result) const {
  std::uint64_t lower_bound = 0;
  std::string val = Uint64ToString(needle);
  PadNeedle(&val);
  return FindWithBounds(val.data(), result, &lower_bound);
}

bool SSTableReader::Find(std::string needle, std::string *result) const {
  PadNeedle(&needle);
  return Find(needle.data(), result);
}

bool SSTableReader::Find(const NGram &ngram, std::string *result) const {
  std::uint64_t lower_bound = 0;
  return FindWithBounds(ngram.data(), ngram.ngram_size, result, &lower_bound);
}

void SSTableReader::PadNeedle(std::string *needle) const {
  std::uint64_t key_size = hdr_.key_size();
  if (needle->size() < key_size) {
    needle->insert(needle->begin(), key_size - needle->size(), '\0');
  }
}
}
