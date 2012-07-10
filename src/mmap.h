// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_MMAP_H_
#define SRC_MMAP_H_

#include <string>

namespace codesearch {
std::pair<std::size_t, const char*> GetMmapForFile(const std::string &name);
void UnmapFiles();
}

#endif  // SRC_MMAP_H_
