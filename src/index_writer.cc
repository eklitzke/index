// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./index_writer.h"
#include "./index.pb.h"
#include "./util.h"

#include <iostream>
#include <fstream>
#include <map>

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

namespace cs {
namespace index {

bool IndexWriter::Initialize() {
  leveldb::Status status;
  leveldb::Options options;
  options.create_if_missing = true;
  options.error_if_exists = true;
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

void IndexWriter::AddFile(const std::string &filename) {
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
    const uint64_t &position_id = item.first;
    const std::string &line = item.second;
    if (line.size() < ngram_size_) {
      continue;
    }
    for (std::string::size_type i = 0; i < line.size() - ngram_size_; i++) {
      std::string ngram = line.substr(i, ngram_size_);
      const auto &map_item = ngrams_map.lower_bound(ngram);
      if (map_item == ngrams_map.end() || map_item->first != ngram) {
        std::vector<std::uint64_t> positions;
        positions.push_back(position_id);
        ngrams_map.insert(map_item, std::make_pair(ngram, positions));
      } else {
        map_item->second.push_back(position_id);
      }
    }
  }

  // We go through the database and for each ngram we have, get the
  // row from the LevelDB database, merge the position ids, and then
  // save the row back.
  leveldb::Status s;
  for (const auto &it : ngrams_map) {
    std::string value;
    NGramValue ngram_val;
    s = ngrams_db_->Get(leveldb::ReadOptions(), it.first, &value);
    if (s.ok()) {
      s = ngrams_db_->Delete(leveldb::WriteOptions(), it.first);
      assert(s.ok());
      ngram_val.ParseFromString(value);
    } else if (s.IsNotFound()) {
      ; // nothing
    } else {
      assert(false);
    }
    MergePostingList(&ngram_val, it.second);
    ngram_val.SerializeToString(&value);
    s = ngrams_db_->Put(leveldb::WriteOptions(), it.first, value);
    assert(s.ok());
  }
}

IndexWriter::~IndexWriter() {
  FinalizeDb(files_db_);
  FinalizeDb(ngrams_db_);
  FinalizeDb(positions_db_);
}

void IndexWriter::MergePostingList(
    NGramValue *existing_val, const std::vector<std::uint64_t> &positions) {
  std::uint64_t last_value = 0;
  for (int i = 0; i < existing_val->position_ids_size(); i++) {
    last_value += existing_val->position_ids(i);
  }
  for (const auto &p : positions) {
    assert(p >= last_value);
    existing_val->add_position_ids(p - last_value);
    last_value = p;
  }
}

void IndexWriter::FinalizeDb(leveldb::DB* db) {
  if (db != nullptr) {
    db->CompactRange(NULL, NULL);
    delete db;
  }
}


}  // index
}  // cs
