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
std::uint64_t ToUint64(const std::array<std::uint8_t, 8> &data);

}
#endif
