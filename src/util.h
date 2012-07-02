// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_UTIL_H_
#define SRC_UTIL_H_

#include <string>
#include <leveldb/options.h>

namespace codesearch {
std::string JoinPath(const std::string &lhs, const std::string &rhs);
leveldb::Options DefaultOptions();
}

#endif
