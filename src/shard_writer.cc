// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./shard_writer.h"
#include "./util.h"

#include <iostream>
#include <fstream>

#include <set>

#include <leveldb/cache.h>
#include <leveldb/options.h>

namespace {
void AddToDatabase(leveldb::DB *db,
                   const google::protobuf::Message &key,
                   const google::protobuf::Message &val) {
  std::string key_string;
  key.SerializeToString(&key_string);
  std::string val_string;
  val.SerializeToString(&val_string);
  leveldb::Status s = db->Put(leveldb::WriteOptions(), key_string, val_string);
  assert(s.ok());
}
}

namespace codesearch {

bool ShardWriter::Initialize() {
  leveldb::Status status;
  leveldb::Options options;
  options.create_if_missing = true;
  options.error_if_exists = true;
  options.compression = leveldb::kSnappyCompression;
  options.write_buffer_size = 40 << 20;
  options.block_size = 1 << 20;
  //options.block_cache = leveldb::NewLRUCache(100 << 20);  // 100MB cache

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

void ShardWriter::AddFile(const std::string &filename) {
  // Add the file to the files database
  std::uint64_t file_id = files_id_.inc();
  FileKey file_key;
  file_key.set_file_id(file_id);
  FileValue file_val;
  file_val.set_filename(filename);
  AddToDatabase(files_db_, file_key, file_val);

  // Collect all of the lines
  std::map<uint64_t, std::string> positions_map;
  {
    std::ifstream ifs(filename.c_str(), std::ifstream::in);
    std::string line;
    std::size_t linenum = 0;
    while (ifs.good()) {
      std::getline(ifs, line);
      std::uint64_t position_id = positions_id_.inc();
      PositionKey key;
      key.set_position_id(position_id);
      PositionValue val;
      val.set_file_id(file_id);
      val.set_file_offset(ifs.tellg());
      val.set_file_line(++linenum);
      val.set_line(line);
      AddToDatabase(positions_db_, key, val);
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

  // We go through the database and for each ngram we have, get the
  // row from the LevelDB database, merge the position ids, and then
  // save the row back.
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

void ShardWriter::FinalizeDb(leveldb::DB* db) {
  if (db != nullptr) {
    db->CompactRange(NULL, NULL);
    delete db;
  }
}
}
