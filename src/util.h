// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_UTIL_H_
#define SRC_UTIL_H_

#include <array>
#include <string>

namespace codesearch {
std::string ConstructShardPath(const std::string &index_directory,
                               const std::string &name,
                               std::uint32_t shard_num);

void Uint64ToString(std::uint64_t val, std::string *out);

template<typename int_type>
std::uint64_t ToUint64(const std::array<int_type,
                       sizeof(std::uint64_t)> &data) {
  return (
    (static_cast<std::uint64_t>(std::get<0>(data)) << 54) +
    (static_cast<std::uint64_t>(std::get<1>(data)) << 48) +
    (static_cast<std::uint64_t>(std::get<2>(data)) << 40) +
    (static_cast<std::uint64_t>(std::get<3>(data)) << 32) +
    (static_cast<std::uint64_t>(std::get<4>(data)) << 24) +
    (static_cast<std::uint64_t>(std::get<5>(data)) << 16) +
    (static_cast<std::uint64_t>(std::get<6>(data)) << 8) +
    (static_cast<std::uint64_t>(std::get<7>(data))));
}

// Get padding to word align something of some size.
std::string GetWordPadding(std::size_t size);

std::string PrintBinaryString(const std::string &str);

}
#endif
