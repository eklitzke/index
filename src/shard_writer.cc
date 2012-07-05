// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./shard_writer.h"
#include "./util.h"
#include "./index.pb.h"

#include <iostream>
#include <fstream>

#include <set>

namespace codesearch {

bool ShardWriter::Initialize() {
  std::string files_db_path = ConstructShardPath(index_directory_,
                                                 "files", shard_num_);
  files_db_ = new SSTableWriter(files_db_path);
  files_db_->Initialize();

  std::string ngrams_db_path = ConstructShardPath(index_directory_,
                                             "ngrams", shard_num_);
  ngrams_db_ = new SSTableWriter(ngrams_db_path);
  ngrams_db_->Initialize();

  std::string positions_db_path = ConstructShardPath(index_directory_,
                                                     "positions", shard_num_);
  positions_db_ = new SSTableWriter(positions_db_path);
  positions_db_->Initialize();

  return true;
}

void ShardWriter::AddFile(const std::string &filename) {
  // Add the file to the files database
  std::uint64_t file_id = files_id_.inc();
  FileValue file_val;
  file_val.set_filename(filename);
  files_db_->Add(file_id, file_val);

  // Collect all of the lines
  std::map<uint64_t, std::string> positions_map;
  {
    std::ifstream ifs(filename.c_str(), std::ifstream::in);
    std::string line;
    std::size_t linenum = 0;
    while (ifs.good()) {
      std::getline(ifs, line);
      std::uint64_t position_id = positions_id_.inc();
      PositionValue val;
      val.set_file_id(file_id);
      val.set_file_offset(ifs.tellg());
      val.set_file_line(++linenum);
      val.set_line(line);
      positions_db_->Add(position_id, val);
      positions_map.insert(std::make_pair(position_id, line));
    }
    ifs.close();
  }

  // We have all of the lines (in memory!) -- generate a map of type
  // ngram -> [position_id]
  std::map<std::string, std::vector<std::uint64_t> > ngrams_map;
  for (const auto &item : positions_map) {
    const uint64_t position_id = item.first;
    const std::string &line = item.second;
    if (line.size() < ngram_size_) {
      continue;
    }
    std::set<std::string> seen_ngrams;
    for (std::string::size_type i = 0; i <= line.size() - ngram_size_; i++) {
      std::string ngram = line.substr(i, ngram_size_);
      auto pos = seen_ngrams.lower_bound(ngram);
      if (pos == seen_ngrams.end() || *pos != ngram) {
        const auto &map_item = ngrams_map.lower_bound(ngram);
        if (map_item == ngrams_map.end() || map_item->first != ngram) {
          std::vector<std::uint64_t> positions;
          positions.push_back(position_id);
          ngrams_map.insert(map_item, std::make_pair(ngram, positions));
        } else {
          map_item->second.push_back(position_id);
        }
        seen_ngrams.insert(pos, ngram);
     }
    }
  }

  for (const auto &it : ngrams_map) {
    ngrams_.UpdateNGram(it.first, it.second);
  }
}

ShardWriter::~ShardWriter() {
  FinalizeDb(files_db_);
  FinalizeDb(positions_db_);

  ngrams_.Serialize(ngrams_db_);
  FinalizeDb(ngrams_db_);
}

void ShardWriter::FinalizeDb(SSTableWriter* db) {
  if (db != nullptr) {
    db->Merge();
    delete db;
  }
}
}
