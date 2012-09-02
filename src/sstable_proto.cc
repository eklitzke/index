// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "sstable_proto.h"

#include <sstream>

namespace codesearch {

std::size_t SSTableValue::SerializeToOstream(std::ostream *os) {
  if (compress_) {
    std::string compress_data;
    snappy::Compress(data_.data(), data_.size(), &compress_data);
    return SerializeString(os, compress_data);
  } else {
    return SerializeString(os, data_);
  }
}

std::string SSTableValue::SerializeToString() {
  std::stringstream ss;
  SerializeToOstream(&ss);
  return ss.str();
}


std::size_t SSTableValue::SerializeString(std::ostream *os,
                                          const std::string &data) {
  assert(!data.empty() && data.size() <= UINT32_MAX);
  size_type be_val = htobe32(data.size());
  os->write(reinterpret_cast<const char *>(&be_val), sizeof(size_type));
  assert(!os->fail());
  os->write(data.data(), data.size());
  assert(!os->fail());
  return sizeof(size_type) + data.size();
}

}  // namespace codesearch
