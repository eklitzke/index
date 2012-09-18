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

class FILEWrapper {
 public:
  FILEWrapper() = delete;
  FILEWrapper(const std::string &name, const std::string &mode)
      :f_(nullptr) {
    f_ = fopen(name.c_str(), mode.c_str());
  }
  FILEWrapper(const FILEWrapper &other) = delete;
  FILEWrapper& operator=(const FILEWrapper &other) = delete;
  ~FILEWrapper() {
    if (f_ != nullptr) {
      fclose(f_);
    }
  }

  bool fail() const { return f_ == nullptr; }

  int fileno() const {
    assert(f_ != nullptr);
    return ::fileno(f_);
  }

  std::size_t size() {
    std::size_t s = lseek(fileno(), 0, SEEK_END);
    rewind(f_);
    return s;
  }

 private:
  FILE* f_;
};

std::pair<std::size_t, void*> DoMmap(const std::string &name) {
  FILEWrapper f(name, "r");
  if (f.fail()) {
    throw std::invalid_argument("failed to mmap(2) file: " + name);
  }
  std::size_t size = f.size();
  void *addr = mmap(nullptr, size, PROT_READ, MAP_SHARED, f.fileno(), 0);
  assert(addr != MAP_FAILED);
#ifdef USE_MADV_RANDOM
  assert(madvise(addr, size, MADV_RANDOM) == 0);
#endif
  return std::make_pair(size, addr);
}
}

namespace codesearch {
MmapCtx::MmapCtx(const std::string &name) {
  auto p = DoMmap(name);
  size_ = p.first;
  mapping_ = p.second;
}

MmapCtx::~MmapCtx() {
  assert(munmap(mapping_, size_) == 0);
}

std::pair<std::size_t, const char*> GetMmapForFile(const std::string &name) {
  std::lock_guard<std::mutex> guard(m_);
  auto it = mapping_.lower_bound(name);
  if (it == mapping_.end() || it->first != name) {
    auto pair = DoMmap(name);
    const char *caddr = reinterpret_cast<const char *>(pair.second);
    std::pair<std::size_t, const char *> p = std::make_pair(pair.first, caddr);
    mapping_.insert(it, std::make_pair(name, p));
    return p;
  }
  return it->second;
}

void UnmapFiles() {
  std::lock_guard<std::mutex> guard(m_);
  for (const auto &p : mapping_) {
    void *void_addr = reinterpret_cast<void *>(
        const_cast<char *>(p.second.second));
    assert(munmap(void_addr, p.second.first) == 0);
  }
  mapping_.clear();
}
}
