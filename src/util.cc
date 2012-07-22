#include "./util.h"

#include <endian.h>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <cstdint>
#include <ctype.h>

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

std::string GetWordPadding(std::size_t size) {
  if (size % 8) {
    return std::string(8 - (size % 8), '\0');
  }
  return "";
}

std::string PrintBinaryString(const std::string &str) {
  std::stringstream ss;

  for (const auto &c : str) {
    if (isgraph(c)) {
      ss << static_cast<char>(c);
    } else {
      ss << "\\x";
      ss << std::setfill('0') << std::setw(2) << std::hex << (
          static_cast<int>(c) & 0xff);
    }
  }
  return ss.str();
}

}
