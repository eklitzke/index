#include "./util.h"

#include <cassert>
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

bool IsValidUtf8(const std::string &src) {
  int n = 0;
  for (const char &i : src) {
    int c = static_cast<int>(i) & 0xFF;
    if (n) {
      if ((c & 0xC0) != 0x80) {
        return false;
      }
      n--;
      continue;
    }
    if (c < 0x80) {
      n = 0; // 0bbbbbbb
    } else if ((c & 0xE0) == 0xC0) {
      n = 1; // 110bbbbb
    } else if ((c & 0xF0) == 0xE0) {
      n = 2; // 1110bbbb
    } else if ((c & 0xF8) == 0xF0) {
      n = 3; // 11110bbb
    } else if ((c & 0xFC) == 0xF8) {
      n = 4; // 111110bb
    } else if ((c & 0xFE) == 0xFC) {
      n = 5; // 1111110b
    } else {
      return false;
    }
  }
  return n == 0;
}

}
