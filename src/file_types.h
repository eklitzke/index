// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_FILE_TYPES_H_
#define SRC_FILE_TYPES_H_

#include <set>
#include <string>

namespace codesearch {
std::string FileLanguage(const std::string &filename);

// Returns true if we should index this file.
bool ShouldIndex(const std::string &filename);
}

#endif
