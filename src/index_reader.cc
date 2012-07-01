// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./index_reader.h"
#include "./index.pb.h"
#include "./util.h"

#include <leveldb/options.h>
#include <iostream>
#include <set>

namespace cs {
namespace index {
bool IndexReader::Initialize() {
  leveldb::Status status;
  leveldb::Options options;
  options.create_if_missing = false;
  options.error_if_exists = false;
  options.compression = leveldb::kSnappyCompression;

  std::string files_db_path = JoinPath(index_directory_, "files");
  status = leveldb::DB::Open(
      options, files_db_path.c_str(), &files_db_);
  if (!status.ok()) {
    return false;
  }

  std::string ngrams_db_path = JoinPath(index_directory_, "ngrams");
  status = leveldb::DB::Open(
      options, ngrams_db_path.c_str(), &ngrams_db_);
  if (!status.ok()) {
    return false;
  }

  std::string positions_db_path = JoinPath(index_directory_, "positions");
  status = leveldb::DB::Open(
      options, positions_db_path.c_str(), &positions_db_);
  if (!status.ok()) {
    return false;
  }
  return true;
}

std::vector<SearchResult> IndexReader::Search(const std::string &query) {
  std::vector<SearchResult> rslt;
  if (query.size() < ngram_size_) {
    return rslt;
  }
  std::string value;
  std::set<std::uint64_t> candidate_positions;
  {
    std::string ngram = query.substr(0, 3);
    leveldb::Status s = ngrams_db_->Get(leveldb::ReadOptions(), ngram, &value);
    if (s.IsNotFound() or !s.ok()){
      return rslt;
    }
    NGramValue ngram_val;
    ngram_val.ParseFromString(value);
    std::uint64_t posting_val = 0;
    for (int i = 0; i < ngram_val.position_ids_size(); i++) {
      posting_val += ngram_val.position_ids(i);
      candidate_positions.insert(candidate_positions.end(), posting_val);
    }
  }
  for (std::string::size_type i = 0;
       i < query.length() - ngram_size_ && !candidate_positions.empty(); i++) {
    std::string ngram = query.substr(i, 3);
    leveldb::Status s = ngrams_db_->Get(leveldb::ReadOptions(), ngram, &value);
    if (s.IsNotFound() or !s.ok()){
      return rslt;
    }
    NGramValue ngram_val;
    ngram_val.ParseFromString(value);
    std::uint64_t posting_val = 0;

    std::set<std::uint64_t> candidates;
    for (int i = 0; i < ngram_val.position_ids_size(); i++) {
      posting_val += ngram_val.position_ids(i);
      candidates.insert(candidates.end(), posting_val);
    }

    // now take the set difference and remove the missing values
    for (auto it = candidate_positions.begin();
         it != candidate_positions.end(); ++it) {
      if (candidates.find(*it) == candidates.end()) {
        candidate_positions.erase(it);
      }
    }
  }

  for (const auto &p : candidate_positions) {
    PositionKey key;
    key.set_position_id(p);
    std::string search_key;
    key.SerializeToString(&search_key);

    PositionValue pos;
    leveldb::Status s = positions_db_->Get(
        leveldb::ReadOptions(), search_key, &value);
    assert(s.ok());
    pos.ParseFromString(value);

    const std::string &pos_line = pos.line();
    if (pos_line.find(query) != std::string::npos) {
      FileKey key;
      key.set_file_id(pos.file_id());
      std::string search_key;
      key.SerializeToString(&search_key);

      leveldb::Status s = files_db_->Get(
          leveldb::ReadOptions(), search_key, &value);
      assert(s.ok());
      FileValue fileval;
      fileval.ParseFromString(value);

      SearchResult r;
      r.filename = fileval.filename();
      r.line_num = pos.file_line();
      r.line_text = pos_line;
      rslt.push_back(r);
    }
  }
  return rslt;
}

IndexReader::~IndexReader() {
  delete files_db_;
  delete ngrams_db_;
  delete positions_db_;
}
}
}
