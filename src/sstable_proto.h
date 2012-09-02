// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_SSTABLE_PROTO_H_
#define SRC_SSTABLE_PROTO_H_

#include <cassert>
#include <istream>
#include <memory>
#include <string>
#include <ostream>

#include <endian.h>
#include <snappy.h>

namespace codesearch {
class SSTableValue {
 public:

  typedef std::uint32_t size_type;

  explicit SSTableValue(const std::string &data, bool compress = true)
      :data_(data), compress_(compress) {}

  std::size_t SerializeToOstream(std::ostream *os);

  std::string SerializeToString();

  template <typename T>
  static SSTableValue Parse(T input, bool compress) {
    std::string data = ParseString(input);
    if (compress) {
      std::string result;
      assert(snappy::Uncompress(data.data(), data.size(), &result) == true);
      return SSTableValue(result, compress);
    } else {
      return SSTableValue(data, compress);
    }
  }

  const std::string& data() const { return data_; }
  bool compress() const { return compress_; }

 private:
  std::string data_;
  bool compress_;

  std::size_t SerializeString(std::ostream *os, const std::string &data);

  static std::string ParseString(std::istream *is) {
    size_type be_size;
    *is >> be_size;
    assert(!is->fail());
    size_type size = be32toh(be_size);

    std::unique_ptr<char[]> buf(new char[size]);
    is->read(buf.get(), size);
    assert(!is->fail());
    return std::string(buf.get(), size);
  }

  static std::string ParseString(const char *buf) {
    const std::uint64_t *raw_size = reinterpret_cast<const std::uint64_t*>(buf);
    std::uint64_t size = be32toh(*raw_size);
    return std::string(buf + sizeof(size_type), size);
  }
};
}

#endif
