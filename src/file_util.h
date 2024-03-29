// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_FILE_UTIL_H_
#define SRC_FILE_UTIL_H_

#include <fstream>
#include <map>
#include <stdexcept>
#include <string>

namespace codesearch {

class FileError : public std::logic_error {
 public:
  FileError(const std::string &msg) :std::logic_error(msg) {}
};

std::string FileLanguage(const std::string &filename);

// Returns true if we should index this file. This applies some
// heuristics to the filename, and if the file isn't blacklisted it
// will read approximately read_size bytes and then apply some
// heuristics on that data to see if it looks like it's mostly UTF-8
// data.
bool ShouldIndex(const std::string &filename, std::size_t read_size = 10000);

// Get the context for a file, where context is the number of
// surrounding lines.
//
// The way this works is you pass in a file name, the line number for
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
std::map<std::size_t, std::string> GetFileContext(const std::string &file_path,
                                                  std::size_t line_number,
                                                  std::uint64_t offset,
                                                  std::size_t context = 1);
}

#endif
