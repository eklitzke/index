#include "./util.h"

#include <endian.h>
#include <sstream>
#include <iostream>
#include <cstdint>
namespace codesearch {
std::string ConstructShardPath(const std::string &index_directory,
                               const std::string &name,
                               std::uint32_t shard_num) {
  std::stringstream ss (std::stringstream::in | std::stringstream::out);
  ss << index_directory << "/" << name << "_" << shard_num;
  return ss.str();
}

void Uint64ToString(std::uint64_t val, std::string *out) {
  if (!out->empty()) {
    out->clear();
  }
  const std::uint64_t be_val = htobe64(val);
  out->assign(reinterpret_cast<const char *>(&be_val), 8);
}

std::uint64_t ToUint64(const std::array<std::uint8_t, 8> &data) {
  return (
      (std::get<0>(data) << 54) +
      (std::get<1>(data) << 48) +
      (std::get<2>(data) << 40) +
      (std::get<3>(data) << 32) +
      (std::get<4>(data) << 24) +
      (std::get<5>(data) << 16) +
      (std::get<6>(data) << 8) +
      (std::get<7>(data)));
}
}
