#include "./mmap.h"

#include <cassert>
#include <string>
#include <map>
#include <mutex>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

namespace {
std::mutex m_;

struct MapEntry {
  int fd;
  const char *addr;
  std::size_t size;
};

std::map<std::string, MapEntry> mapping_;
}

namespace codesearch {
std::pair<std::size_t, const char*> GetMmapForFile(const std::string &name) {
  std::lock_guard<std::mutex> guard(m_);
  auto it = mapping_.lower_bound(name);
  if (it == mapping_.end() || it->first != name) {
    FILE *f = fopen(name.c_str(), "r");
    assert(f != nullptr);
    int fd = fileno(f);

    std::size_t size = lseek(fd, 0, SEEK_END);
    rewind(f);

    void *addr = mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);
#ifdef USE_MADV_RANDOM
    madvise(addr, size, MADV_RANDOM);
#endif

    MapEntry entry;
    entry.fd = fd;
    entry.addr = reinterpret_cast<const char *>(addr);
    entry.size = size;
    mapping_.insert(it, std::make_pair(name, entry));
    return std::make_pair(entry.size, entry.addr);
  }
  return std::make_pair(it->second.size, it->second.addr);
}

void UnmapFiles() {
  std::lock_guard<std::mutex> guard(m_);
  for (const auto &p : mapping_) {
    close(p.second.fd);
    munmap(reinterpret_cast<void *>(
        const_cast<char *>(p.second.addr)), p.second.size);
  }
  mapping_.clear();
}
}
