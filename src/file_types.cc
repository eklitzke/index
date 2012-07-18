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
std::string file_types_regex() {
  DemandInitialize();
  std::string regex(".*\\.(");
  for (const auto &p : file_types_) {
    regex += p.first + "|";
  }
  regex.resize(regex.size() - 1);  // take off the trailing pipe
  regex += ")$";
  return regex;
}

std::string FileLanguage(const std::string &filename) {
  DemandInitialize();
  return "";
}
}
