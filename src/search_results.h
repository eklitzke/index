// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_SEARCH_RESULTS_H_
#define SRC_SEARCH_RESULTS_H_

#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "./bounded_map.h"
#include "./index.pb.h"

namespace codesearch {

class FileKey {
 public:
  FileKey(std::uint64_t file_id = 0, const std::string &filename = "")
      :file_id_(file_id), filename_(filename) {}

  std::uint64_t file_id() const { return file_id_; }
  std::string filename() const { return filename_; }

  inline bool operator <(const FileKey &other) const {
    return file_id_ < other.file_id_;
  }

  inline bool operator <=(const FileKey &other) const {
    return file_id_ <= other.file_id_;
  }

  inline bool operator ==(const FileKey &other) const {
    return file_id_ == other.file_id_;
  }

  inline bool operator !=(const FileKey &other) const {
    return file_id_ != other.file_id_;
  }

  inline bool operator >(const FileKey &other) const {
    return file_id_ > other.file_id_;
  }

  inline bool operator >=(const FileKey &other) const {
    return file_id_ >= other.file_id_;
  }

 protected:
  std::uint64_t file_id_;
  std::string filename_;
};
}

namespace std {
template <> struct hash<codesearch::FileKey> {
  size_t operator()(const codesearch::FileKey &k) const {
    return hash<uint64_t>()(k.file_id());
  }
};
}

namespace codesearch {

struct FileResult {
  FileResult(std::size_t off, std::size_t line_num)
      :offset(off), line_number(line_num) {}

  std::size_t offset;
  std::size_t line_number;

  // Needed for std::sort
  bool operator <(const FileResult &other) const {
    return offset < other.offset;
  }
};

class SearchResults : public BoundedMap<FileKey, FileResult> {
 public:
  SearchResults(std::size_t a, std::size_t b)
      :BoundedMap(a, b) {}
  std::vector<SearchResultContext> contextual_results();
};

}  // namespace codesearch


#endif
