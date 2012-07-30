// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_FILE_UTIL_H_
#define SRC_FILE_UTIL_H_

#include <fstream>
#include <map>
#include <string>

namespace codesearch {
std::string FileLanguage(const std::string &filename);

// Returns true if we should index this file.
bool ShouldIndex(const std::string &filename);

// Get the context for a file, where context is the number of
// surrounding lines.
//
// The way this works is you pass in an ifstream, the line number for
// the context, and the offset into the file where that line
// starts. You also say how many lines of context you want. Then a map
// of line number -> line is returned. For instance, suppose we have
// the following file (line-delimited by \n, and file contents
// delimited by """):
//
// """
// foo
// bar
// baz
// quux
// """
//
// If we call GetFileContext(f, 0, 0, 1) then the return value of this
// function will be {0: "foo", 1: "bar"}. If we call GetFileContext(f,
// 1, 4, 1) then the return value will be {0: "foo", 1: "bar", 2:
// "baz"}. If we call GetFileContext(f, 3, 12, 1) then the return
// value will be {2: "baz", 3: "quux"}.
//
// Note that the line number you pass in is not checked, so it's up to
// you if you think the lines should be 0 offset, 1 offset, etc.
std::map<std::size_t, std::string> GetFileContext(std::ifstream *ifs,
                                                  std::size_t line_number,
                                                  std::uint64_t offset,
                                                  std::size_t context = 1);

// Helper that opens the file for reading.
std::map<std::size_t, std::string> GetFileContext(const std::string &file_path,
                                                  std::size_t line_number,
                                                  std::uint64_t offset,
                                                  std::size_t context = 1);
}

#endif
