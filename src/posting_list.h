// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_POSTING_LIST_H_
#define SRC_POSTING_LIST_H_

#include <map>
#include <string>
#include <vector>
#include <leveldb/db.h>

namespace codesearch {
class PostingList {
 public:
  PostingList() {}
  void UpdateNGram(const std::string &ngram,
                   const std::vector<std::uint64_t> &ids);
  void Serialize(leveldb::DB *db);
 private:
  std::map<std::string, std::vector<std::uint64_t> > lists_;
};
}

#endif // SRC_POSTING_LIST_H_
