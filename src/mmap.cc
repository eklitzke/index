// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./mmap.h"

#include <cassert>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>

#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

namespace {
std::mutex m_;
std::map<std::string, std::pair<std::size_t, const char*> > mapping_;
}

namespace codesearch {
std::pair<std::size_t, const char*> GetMmapForFile(const std::string &name) {
  std::lock_guard<std::mutex> guard(m_);
  auto it = mapping_.lower_bound(name);
  if (it == mapping_.end() || it->first != name) {
    FILE *f = fopen(name.c_str(), "r");
    if (f == nullptr) {
      throw std::invalid_argument("failed to mmap(2) file: " + name);
    }
    int fd = fileno(f);

    std::size_t size = lseek(fd, 0, SEEK_END);
    rewind(f);

    void *addr = mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);
#ifdef USE_MADV_RANDOM
    madvise(addr, size, MADV_RANDOM);
#endif
    fclose(f);

    const char *caddr = reinterpret_cast<const char *>(addr);
    std::pair<std::size_t, const char *> p = std::make_pair(size, caddr);
    mapping_.insert(it, std::make_pair(name, p));
    return p;
  }
  return it->second;
}

void UnmapFiles() {
  std::lock_guard<std::mutex> guard(m_);
  for (const auto &p : mapping_) {
    munmap(reinterpret_cast<void *>(
        const_cast<char *>(p.second.second)), p.second.first);
  }
  mapping_.clear();
}
}
