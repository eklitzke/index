// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./shard_reader.h"
#include "./util.h"

#include <algorithm>
#include <iostream>
#include <fstream>
#include <set>

namespace codesearch {

bool ShardReader::Initialize() {
  std::string files_db_path = ConstructShardPath(index_directory_,
                                                 "files", shard_num_);
  files_db_ = new SSTableReader(files_db_path);
  files_db_->Initialize();

  std::string ngrams_db_path = ConstructShardPath(index_directory_,
                                                  "ngrams", shard_num_);
  ngrams_db_ = new SSTableReader(ngrams_db_path);
  ngrams_db_->Initialize();

  std::string positions_db_path = ConstructShardPath(index_directory_,
                                                     "positions", shard_num_);
  positions_db_ = new SSTableReader(positions_db_path);
  positions_db_->Initialize();

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
    PositionValue pos;
    std::string value;
    positions_db_->Find(p, &value);
    pos.ParseFromString(value);

    const std::string &pos_line = pos.line();
    if (pos_line.find(query) != std::string::npos) {
      files_db_->Find(pos.file_id(), &value);
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
  std::string db_read;
  bool found = ngrams_db_->Find(ngram, &db_read);
  if (!found) {
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
