// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./file_types.h"

#include <map>

namespace {
bool initialized_ = false;
std::map<std::string, std::string> file_types_;

void DemandInitialize() {
  if (!initialized_) {
    file_types_.insert(std::make_pair("C", "c++"));
    file_types_.insert(std::make_pair("ada", "ada"));
    file_types_.insert(std::make_pair("bash", "shell"));
    file_types_.insert(std::make_pair("c", "c"));
    file_types_.insert(std::make_pair("cc", "c++"));
    file_types_.insert(std::make_pair("clj", "clojure"));
    file_types_.insert(std::make_pair("cpp", "c++"));
    file_types_.insert(std::make_pair("css", "css"));
    file_types_.insert(std::make_pair("go", "go"));
    file_types_.insert(std::make_pair("h", "c"));
    file_types_.insert(std::make_pair("hh", "c++"));
    file_types_.insert(std::make_pair("hpp", "c++"));
    file_types_.insert(std::make_pair("hs", "haskell"));
    file_types_.insert(std::make_pair("html", "html"));
    file_types_.insert(std::make_pair("ipp", "c++"));
    file_types_.insert(std::make_pair("java", "java"));
    file_types_.insert(std::make_pair("js", "javascript"));
    file_types_.insert(std::make_pair("json", "javascript"));
    file_types_.insert(std::make_pair("lua", "lua"));
    file_types_.insert(std::make_pair("m", "obj-c"));
    file_types_.insert(std::make_pair("m4", "m4"));
    file_types_.insert(std::make_pair("md", "text"));
    file_types_.insert(std::make_pair("mm", "obj-c"));
    file_types_.insert(std::make_pair("pl", "perl"));
    file_types_.insert(std::make_pair("pm", "perl"));
    file_types_.insert(std::make_pair("py", "python"));
    file_types_.insert(std::make_pair("rb", "ruby"));
    file_types_.insert(std::make_pair("sh", "shell"));
    file_types_.insert(std::make_pair("txt", "text"));
    file_types_.insert(std::make_pair("xhtml", "html"));
    file_types_.insert(std::make_pair("xml", "xml"));
    file_types_.insert(std::make_pair("yaml", "yaml"));
    initialized_ = true;
  }
}
}

namespace codesearch {
std::string FileLanguage(const std::string &filename) {
  DemandInitialize();
  std::string::size_type dotpos = filename.find_last_of('.');
  if (dotpos == std::string::npos) {
    dotpos = 0;
  } else {
    dotpos++;
  }
  std::string extension = filename.substr(dotpos, std::string::npos);

  const auto &it = file_types_.find(extension);
  if (it == file_types_.end()) {
    return "";
  } else {
    return it->second;
  }
}

bool ShouldIndex(const std::string &filename) {
  return !FileLanguage(filename).empty();
}
}
