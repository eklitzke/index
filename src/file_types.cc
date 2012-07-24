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
    file_types_.insert(std::make_pair("LICENSE", "text"));
    file_types_.insert(std::make_pair("Makefile", "make"));
    file_types_.insert(std::make_pair("NOTICE", "text"));
    file_types_.insert(std::make_pair("README", "text"));
    file_types_.insert(std::make_pair("S", "asm"));
    file_types_.insert(std::make_pair("ada", "ada"));
    file_types_.insert(std::make_pair("am", "make"));
    file_types_.insert(std::make_pair("asm", "asm"));
    file_types_.insert(std::make_pair("bash", "shell"));
    file_types_.insert(std::make_pair("bat", "shell"));
    file_types_.insert(std::make_pair("c", "c"));
    file_types_.insert(std::make_pair("cc", "c++"));
    file_types_.insert(std::make_pair("clj", "clojure"));
    file_types_.insert(std::make_pair("conf", "text"));
    file_types_.insert(std::make_pair("cpp", "c++"));
    file_types_.insert(std::make_pair("cs", "csharp"));
    file_types_.insert(std::make_pair("css", "css"));
    file_types_.insert(std::make_pair("el", "elisp"));
    file_types_.insert(std::make_pair("gitignore", "text"));
    file_types_.insert(std::make_pair("go", "go"));
    file_types_.insert(std::make_pair("h", "c"));
    file_types_.insert(std::make_pair("hh", "c++"));
    file_types_.insert(std::make_pair("hpp", "c++"));
    file_types_.insert(std::make_pair("hs", "haskell"));
    file_types_.insert(std::make_pair("htm", "html"));
    file_types_.insert(std::make_pair("html", "html"));
    file_types_.insert(std::make_pair("in", "make"));
    file_types_.insert(std::make_pair("ipp", "c++"));
    file_types_.insert(std::make_pair("java", "java"));
    file_types_.insert(std::make_pair("js", "javascript"));
    file_types_.insert(std::make_pair("json", "javascript"));
    file_types_.insert(std::make_pair("lua", "lua"));
    file_types_.insert(std::make_pair("m", "obj-c"));
    file_types_.insert(std::make_pair("m4", "m4"));
    file_types_.insert(std::make_pair("man", "man"));
    file_types_.insert(std::make_pair("md", "text"));
    file_types_.insert(std::make_pair("mk", "make"));
    file_types_.insert(std::make_pair("ml", "ml"));
    file_types_.insert(std::make_pair("mm", "obj-c"));
    file_types_.insert(std::make_pair("php", "php"));
    file_types_.insert(std::make_pair("pl", "perl"));
    file_types_.insert(std::make_pair("pm", "perl"));
    file_types_.insert(std::make_pair("proto", "protobuf"));
    file_types_.insert(std::make_pair("py", "python"));
    file_types_.insert(std::make_pair("rb", "ruby"));
    file_types_.insert(std::make_pair("s", "asm"));
    file_types_.insert(std::make_pair("sh", "shell"));
    file_types_.insert(std::make_pair("sql", "sql"));
    file_types_.insert(std::make_pair("tex", "tex"));
    file_types_.insert(std::make_pair("txt", "text"));
    file_types_.insert(std::make_pair("vb", "vb"));
    file_types_.insert(std::make_pair("vbs", "vb"));
    file_types_.insert(std::make_pair("xhtml", "html"));
    file_types_.insert(std::make_pair("xml", "xml"));
    file_types_.insert(std::make_pair("xsd", "xml"));
    file_types_.insert(std::make_pair("yaml", "yaml"));
    file_types_.insert(std::make_pair("yml", "yaml"));
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
