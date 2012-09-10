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
    :name_(name) {
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
}
}
