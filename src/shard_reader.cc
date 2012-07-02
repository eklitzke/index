// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./shard_reader.h"
#include "./util.h"

#include <algorithm>
#include <iostream>
#include <fstream>
#include <set>

#include <leveldb/cache.h>
#include <leveldb/options.h>

namespace codesearch {

bool ShardReader::Initialize() {
  leveldb::Status status;
  leveldb::Options options = DefaultOptions();
  options.create_if_missing = false;
  options.error_if_exists = false;

  std::string files_db_path = ConstructShardPath(index_directory_,
                                                 "files", shard_num_);
  status = leveldb::DB::Open(
      options, files_db_path.c_str(), &files_db_);
  if (!status.ok()) {
    return false;
  }

  std::string ngrams_db_path = ConstructShardPath(index_directory_,
                                                  "ngrams", shard_num_);
  status = leveldb::DB::Open(
      options, ngrams_db_path.c_str(), &ngrams_db_);
  if (!status.ok()) {
    return false;
  }

  std::string positions_db_path = ConstructShardPath(index_directory_,
                                                     "positions", shard_num_);
  status = leveldb::DB::Open(
      options, positions_db_path.c_str(), &positions_db_);
  if (!status.ok()) {
    return false;
  }
  return true;
}

bool ShardReader::Search(const std::string &query) {
  results_.Reset();
  if (query.size() < ngram_size_) {
    return true;
  }
  std::string value;
  std::vector<std::uint64_t> candidates;
  std::vector<std::uint64_t> intersection;
  if (!GetCandidates(query.substr(0, ngram_size_), &candidates)) {
    return false;
  }

  for (std::string::size_type i = 1;
       i <= query.length() - ngram_size_ && !candidates.empty(); i++) {
    std::vector<std::uint64_t> new_candidates;
    if (!GetCandidates(query.substr(i, ngram_size_), &new_candidates)) {
      return false;
    }

    intersection.clear();
    intersection.reserve(std::min(candidates.size(), new_candidates.size()));
    auto it = std::set_intersection(
        candidates.begin(), candidates.end(),
        new_candidates.begin(), new_candidates.end(),
        intersection.begin());
#if 0
    candidates.swap(intersection);
#endif
    candidates.clear();
    candidates.reserve(it - intersection.begin());
    candidates.insert(candidates.begin(), intersection.begin(), it);
  }

  for (const auto &p : candidates) {
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

      results_.AddResult(fileval.filename(), pos.file_line(), pos_line);
    }
  }
  return true;
}

bool ShardReader::GetCandidates(const std::string &ngram,
                                std::vector<std::uint64_t> *candidates) {
  assert(candidates->empty());
  std::string key_slice, val_slice, db_read;
  NGramKey key;
  key.set_ngram(ngram);
  key.SerializeToString(&key_slice);
  leveldb::Status s = ngrams_db_->Get(
      leveldb::ReadOptions(), key_slice, &db_read);
  if (!s.ok() && !s.IsNotFound()) {
    return false;
  }

  NGramValue ngram_val;
  ngram_val.ParseFromString(db_read);
  std::uint64_t posting_val = 0;
  for (int i = 0; i < ngram_val.position_ids_size(); i++) {
    std::uint64_t delta = ngram_val.position_ids(i);
    if (delta != 0) {
      posting_val += delta;
      candidates->push_back(posting_val);
    } else {
      std::cerr << "warning, 0 seen in posting list" << std::endl;
    }
  }
  return true;
}

ShardReader::~ShardReader() {
  delete files_db_;
  delete ngrams_db_;
  delete positions_db_;
}
}
