// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_MMAP_H_
#define SRC_MMAP_H_

#include <string>

namespace codesearch {
class MmapCtx {
 public:
  explicit MmapCtx(const std::string &name);
  ~MmapCtx();

  std::size_t size() const { return size_; }
  const char* mapping() const { return static_cast<const char*>(mapping_); }

 private:
  std::size_t size_;
  void *mapping_;
};

std::pair<std::size_t, const char*> GetMmapForFile(const std::string &name);
void UnmapFiles();
}

#endif  // SRC_MMAP_H_
