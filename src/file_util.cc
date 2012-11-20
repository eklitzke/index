// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./file_util.h"

#include "./context.h"
#include "./mmap.h"
#include "./util.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace {
const std::unordered_map<std::string, std::string> file_types_{
  {"C", "c++"},
  {"LICENSE", "text"},
  {"Makefile", "make"},
  {"NOTICE", "text"},
  {"README", "text"},
  {"S", "asm"},
  {"ada", "ada"},
  {"am", "make"},
  {"asm", "asm"},
  {"bash", "shell"},
  {"bat", "shell"},
  {"c", "c"},
  {"cc", "c++"},
  {"clj", "clojure"},
  {"conf", "text"},
  {"cpp", "c++"},
  {"cs", "csharp"},
  {"css", "css"},
  {"el", "elisp"},
  {"gitignore", "text"},
  {"go", "go"},
  {"h", "c"},
  {"hh", "c++"},
  {"hpp", "c++"},
  {"hs", "haskell"},
  {"htm", "html"},
  {"html", "html"},
  {"in", "make"},
  {"ipp", "c++"},
  {"java", "java"},
  {"js", "javascript"},
  {"json", "javascript"},
  {"lua", "lua"},
  {"m", "obj-c"},
  {"m4", "m4"},
  {"man", "man"},
  {"md", "text"},
  {"mk", "make"},
  {"ml", "ml"},
  {"mm", "obj-c"},
  {"php", "php"},
  {"pl", "perl"},
  {"pm", "perl"},
  {"proto", "protobuf"},
  {"py", "python"},
  {"rb", "ruby"},
  {"s", "asm"},
  {"sh", "shell"},
  {"sql", "sql"},
  {"tex", "tex"},
  {"txt", "text"},
  {"vb", "vb"},
  {"vbs", "vb"},
  {"xhtml", "html"},
  {"xml", "xml"},
  {"xsd", "xml"},
  {"yaml", "yaml"},
  {"yml", "yaml"},
};

const std::vector<std::string> bad_dirs_{".git", ".hg", ".repo", ".svn"};

// These are files that we never, ever want to index. Some of these
// are here just because we know they're always binary and it doesn't
// make sense to read a bunch of data and apply our UTF-8 heuristics;
// others, like pdf, are here because the heuristics sometimes mark
// them as text even though they are binary.
const std::vector<std::string> bad_exts_{
  "a", "jpeg", "jpg", "mo", "o", "pdf", "png", "so", "swp"};

// Get the extension of a file. For instance,
//  foo.png   -> png
//  Makefile  -> Makefile
std::string GetExtension(const std::string &filename) {
  std::string::size_type dotpos = filename.find_last_of('.');
  if (dotpos == std::string::npos) {
    dotpos = 0;
  } else {
    dotpos++;
  }
  return filename.substr(dotpos, std::string::npos);
}

inline const char* c_memchr(const char *s, int c, size_t n) {
  return reinterpret_cast<const char*>(
      memchr(reinterpret_cast<const void*>(s), c, n));
}

inline const char* c_memrchr(const char *s, int c, size_t n) {
  return reinterpret_cast<const char*>(
      memrchr(reinterpret_cast<const void*>(s), c, n));
}

std::string GetLineBackwards(const char *buf, std::size_t *position) {
  assert(*position && buf[*position - 1] == '\n');
#if 0
  if (*position == 1) {
    *position = 0;
    return std::string{};
  }
#endif
  std::string line;
  std::size_t newpos;
  const char *newbuf = c_memrchr(buf, '\n', *position - 1);
  if (newbuf == nullptr) {
    newpos = 0;
    line = std::string(buf, *position - 1);
  } else {
    newpos = newbuf - buf + 1;
    line = std::string(newbuf + 1, *position - newpos - 1);
  }
  assert(line.find_first_of('\n') == std::string::npos);
  *position = newpos;
  return line;
}

std::string GetLineForwards(const char *buf, std::size_t buf_size,
                            std::size_t *position) {
  assert(*position < buf_size);
  assert(!*position || buf[*position - 1] == '\n');

  std::string line;
  std::size_t newpos;
  const char *newbuf = c_memchr(
      buf + *position, '\n', buf_size - *position - 1);
  if (newbuf == nullptr) {
    newpos = buf_size;
    line = std::string(buf + *position, buf_size - *position);
    if (line.size() && line[line.size() - 1] == '\n') {
      line = line.substr(0, line.size() - 1);
    }
  } else {
    newpos = newbuf - buf + 1;
    line = std::string(buf + *position, newpos - *position - 1);
  }
  assert(line.find_first_of('\n') == std::string::npos);
  *position = newpos;
  return line;
}
}

namespace codesearch {
std::string FileLanguage(const std::string &filename) {
  std::string extension = GetExtension(filename);
  const auto &it = file_types_.find(extension);
  if (it == file_types_.end()) {
    return "";
  } else {
    return it->second;
  }
}

bool ShouldIndex(const std::string &filename, std::size_t read_size) {
  // Try to detect files in directories like .git, .hg, etc.
  for (const auto &dirname : bad_dirs_) {
    if (filename.find("/" + dirname + "/") != std::string::npos) {
      return false;
    }
  }

  // Detect bad file extensions, like .pdf
  const std::string extension = GetExtension(filename);
  if (std::binary_search(bad_exts_.cbegin(), bad_exts_.cend(), extension)) {
    return false;
  }

  // We're going to read the first 10kish bytes of the file (breaking
  // on newline boundaries), and detect what % of the data looks like
  // it's UTF-8; if we get 95% or more valid UTF-8 data, then we
  // choose to index the file.
  std::ifstream ifs(filename, std::ifstream::in | std::ifstream::binary);
  assert(!ifs.fail());
  std::string s;
  std::size_t valid_data = 0;
  std::size_t invalid_data = 0;
  while (true) {
    std::getline(ifs, s);
    if (IsValidUtf8(s)) {
      valid_data += s.size();
    } else {
      invalid_data += s.size();
    }
    if (ifs.eof() || ifs.tellg() == -1) {
      break;
    }
    if (static_cast<std::size_t>(ifs.tellg()) >= read_size) {
      break;
    }
    assert(!ifs.fail());
    s.clear();  // is this necessary?
  }
  return valid_data > 0 && valid_data >= 20 * invalid_data;
}

std::map<std::size_t, std::string> GetFileContext(const std::string &name,
                                                  std::size_t line_number,
                                                  std::uint64_t offset,
                                                  std::size_t context) {
  MmapCtx memory_map(name);
  const char *mmap_addr = memory_map.mapping();
  const std::size_t map_size = memory_map.size();
  std::map<std::size_t, std::string> return_map;

  // We are going to maintain an invariant that at the start of this
  // function, and after each loop iteration (both forwards and
  // backwards), the position of the cursor will be at a position
  // indicated by a caret mark below:
  //
  //  foo\nbar\nbaz\n\nquux
  //  ^    ^    ^     ^^
  //
  // That is, at each point, the previous character is a newline (or
  // the start of the file).

  std::uint64_t pos = offset;

  // Check the invariant
  assert(pos == 0 || mmap_addr[pos - 1] == '\n');

  if (pos != 0) {
    for (std::size_t i = 0; i < context; i++) {
      std::string line = GetLineBackwards(mmap_addr, &pos);
      return_map.insert(return_map.begin(), {line_number - i - 1, line});
      if (pos == 0) {
        break;
      }
    }
  }

  pos = offset;
  if (pos < map_size - 1) {
    for (std::size_t i = 0; i < context + 1; i++) {
      std::string line = GetLineForwards(mmap_addr, map_size, &pos);
      return_map.insert(return_map.begin(), {line_number + i, line});
      if (pos >= map_size) {
        break;
      }
    }
  }

  return return_map;
}
}
