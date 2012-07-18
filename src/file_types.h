// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_FILE_TYPES_H_
#define SRC_FILE_TYPES_H_

#include <string>

namespace codesearch {
std::string file_types_regex();
std::string FileLanguage(const std::string &filename);
}

#endif
