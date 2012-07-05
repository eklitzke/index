// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./sstable_reader.h"
#include "util.h"

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
void SSTableReader::Initialize() {
  std::string sst_name = name_ + ".sst";
  index_file_ = fopen(sst_name.c_str(), "r");
  assert(index_file_ != nullptr);
  fseek(index_file_, 0, SEEK_END);
  std::size_t file_size = ftell(index_file_);
  rewind(index_file_);

  std::array<std::uint8_t, 8> size_field;
  assert(fread(size_field.data(), 1, 8, index_file_) == 8);
  index_len_ = ToUint64(size_field);

  assert(fread(size_field.data(), 1, 8, index_file_) == 8);
  data_len_ = ToUint64(size_field);

  assert(file_size == 16 + index_len_ + data_len_);
  void *mmap_addr = mmap(nullptr, file_size, PROT_READ, MAP_SHARED,
                         fileno(index_file_), 0);
  assert(mmap_addr != MAP_FAILED);
  mmap_addr_ = static_cast<const char *>(mmap_addr);

#ifdef USE_MADV_RANDOM
  madvise(mmap_addr, file_size, MADV_RANDOM);
#endif

  assert(index_len_ % 16 == 0);
}

bool SSTableReader::FindWithBounds(const char *needle, std::string *result,
                                   std::size_t *lower_bound) {
  std::size_t upper = upper_bound();
  while (*lower_bound <= upper) {
    std::size_t pos = (upper + *lower_bound) / 2;
    const char *key = mmap_addr_ + (pos << 4) + 16;
    int cmpresult = memcmp(needle, key, 8);
    if (cmpresult < 0) {
      upper = pos - 1;
    } else if (cmpresult > 0) {
      *lower_bound = pos + 1;
    } else {
      const std::uint64_t *raw_offset = reinterpret_cast<const std::uint64_t*>(
          mmap_addr_ + (pos << 4) + 24);
      std::uint64_t data_offset = be64toh(*raw_offset);
      const std::uint64_t *raw_size = reinterpret_cast<const std::uint64_t*>(
          mmap_addr_ + index_len_ + data_offset + 16);
      std::uint64_t data_size = be64toh(*raw_size);
      const char *data_loc = mmap_addr_ + index_len_ + data_offset + 24;

#ifdef USE_SNAPPY
      assert(snappy::Uncompress(data_loc, data_size, result) == true);
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
                                   std::size_t *lower_bound) {
  std::string padded;
  PadString(needle, &padded);
  return FindWithBounds(padded.c_str(), result, lower_bound);
}

bool SSTableReader::Find(const char *needle, std::string *result) {
  std::size_t lower_bound = 0;
  return FindWithBounds(needle, result, &lower_bound);
}

bool SSTableReader::Find(std::uint64_t needle, std::string *result) {
  std::string val;
  Uint64ToString(needle, &val);
  return Find(val.c_str(), result);
}

bool SSTableReader::Find(const std::string &needle, std::string *result) {
  std::string padded;
  PadString(needle, &padded);
  return Find(padded.c_str(), result);
}

void SSTableReader::PadString(const std::string &in, std::string *out) {
  assert(in.size() <= 8);
  *out = std::string(8 - in.size(), '\0') + in;
}

SSTableReader::~SSTableReader() {
  if (mmap_addr_ != nullptr) {
    munmap(const_cast<void*>(
        reinterpret_cast<const void *>(mmap_addr_)), index_len_ + data_len_);
  }
  if (index_file_ != nullptr) {
    fclose(index_file_);
  }
}
}
