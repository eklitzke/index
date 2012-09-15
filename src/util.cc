#include "./util.h"

#include <fstream>
#include <memory>

namespace codesearch {
std::string ConstructShardPath(const std::string &index_directory,
                               const std::string &name,
                               std::uint32_t shard_num) {
  std::stringstream ss (std::stringstream::in | std::stringstream::out);
  ss << index_directory << "/" << name << "_" << shard_num;
  return ss.str();
}

std::uint64_t FileSize(std::istream *is) {
  std::streampos p = is->tellg();
  assert(p >= 0);
  is->seekg(0, std::ifstream::end);
  std::streampos endp = is->tellg();
  assert(endp >= 0);
  std::uint64_t file_size = static_cast<std::uint64_t>(endp);
  is->seekg(p);
  assert(!is->fail());
  return file_size;
}

SSTableHeader ReadHeader(std::istream *is) {
  is->seekg(0, std::ifstream::end);
  std::streampos file_size = is->tellg();
  assert(file_size >= 0);
  is->seekg(0);
  std::uint64_t hdr_size = ReadUint64(is);
  assert(hdr_size < 8096); // ensure the size is plausible

  std::unique_ptr<char []> hdr_data(new char[hdr_size]);
  is->read(hdr_data.get(), hdr_size);
  assert(!is->eof() && !is->fail());

  std::string hdr_string(hdr_data.get(), hdr_size);
  codesearch::SSTableHeader hdr;
  hdr.ParseFromString(hdr_string);
  return hdr;
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
