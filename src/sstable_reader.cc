// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./sstable_reader.h"
#include "./util.h"
#include "./mmap.h"

#include <array>
#include <cassert>
#include <cstring>
#include <memory>
#include <iostream>

#ifdef USE_SNAPPY
#include <snappy.h>
#endif
#include <sys/mman.h>

namespace codesearch {
SSTableReader::SSTableReader(const std::string &name) {
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

  // point the mmap at the start of the index
  mmap_addr_ += sizeof(std::uint64_t) + hdr_size + padding.size();
}

bool SSTableReader::FindWithBounds(const char *needle, std::string *result,
                                   std::size_t *lower_bound) const {
  std::uint64_t key_size = hdr_.key_size();

  // First we check that the needle being searched for is within the
  // min/max values stored in this SSTable.
  if (memcmp(static_cast<const void *>(needle),
             static_cast<const void *>(hdr_.min_value().data()),
             key_size) < 0) {
    return false;
  } else if (memcmp(static_cast<const void *>(needle),
                    static_cast<const void *>(hdr_.max_value().data()),
                    key_size) > 0) {
    *lower_bound = upper_bound() + 1;
    return false;
  }

  std::size_t upper = upper_bound();
  while (*lower_bound <= upper) {
    std::size_t pos = (upper + *lower_bound) / 2;
    const char *key = mmap_addr_ + pos * (key_size + sizeof(std::uint64_t));
    int cmpresult = memcmp(needle, key, key_size);
    if (cmpresult < 0) {
      upper = pos - 1;
    } else if (cmpresult > 0) {
      *lower_bound = pos + 1;
    } else {
      const std::uint64_t *position = reinterpret_cast<const std::uint64_t*>(
          key + key_size);
      std::uint64_t data_offset = be64toh(*position);
      const std::uint64_t *raw_size = reinterpret_cast<const std::uint64_t*>(
          mmap_addr_ + hdr_.index_size() + data_offset);
      std::uint64_t data_size = be64toh(*raw_size);
      assert(data_size != 0);
      const char *data_loc = (reinterpret_cast<const char*>(raw_size) +
                              sizeof(std::uint64_t));

#ifdef USE_SNAPPY
      assert(snappy::Uncompress(data_loc, data_size, result) == true);
      assert(result->size() != 0);
#if 0
      std::cout << "uncompressed data of size " << result->size() << std::endl;
#endif
#else
      result->assign(data_loc, data_size);
#endif
      return true;
     }
  }
  return false;
}

bool SSTableReader::FindWithBounds(const std::string &needle,
                                   std::string *result,
                                   std::size_t *lower_bound) const {
  std::string padded;
  PadNeedle(needle, &padded);
  return FindWithBounds(padded.c_str(), result, lower_bound);
}

bool SSTableReader::Find(const char *needle, std::string *result) const {
  std::size_t lower_bound = 0;
  return FindWithBounds(needle, result, &lower_bound);
}

bool SSTableReader::Find(std::uint64_t needle, std::string *result) const {
  std::string val;
  Uint64ToString(needle, &val);
  return Find(val.c_str(), result);
}

bool SSTableReader::Find(const std::string &needle, std::string *result) const {
  std::string padded;
  PadNeedle(needle, &padded);
  return Find(padded.c_str(), result);
}

void SSTableReader::PadNeedle(const std::string &in, std::string *out) const {
  std::uint64_t key_size = hdr_.key_size();
  if (in.size() == key_size) {
    *out = in;
  } else if (in.size() < key_size) {
    *out = std::string(key_size - in.size(), '\0') + in;
  } else {
    assert(false);
  }
}
}
