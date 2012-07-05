// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./index_writer.h"
#include "./util.h"

#include <iostream>
#include <fstream>
#include <map>
#include <set>

#include "./index.pb.h"

namespace codesearch {
bool IndexWriter::Initialize() {
  WriteStatus(IndexConfig_DatabaseState_EMPTY);
  shards_.reserve(database_parallelism_);
  for (std::uint32_t i = 0; i < database_parallelism_; i++) {
    ShardWriter *shard = new ShardWriter(index_directory_, ngram_size_, i);
    if (!shard->Initialize()) {
      return false;
    }
    shards_.push_back(shard);
  }
  return true;
}

void IndexWriter::AddFile(const std::string &filename) {
  shards_[next_shard_]->AddFile(filename);
  next_shard_ = (next_shard_ + 1) % database_parallelism_;
}

IndexWriter::~IndexWriter() {
  for (auto &shard : shards_) {
    delete shard;
  }
  WriteStatus(IndexConfig_DatabaseState_COMPLETE);
}

void IndexWriter::WriteStatus(IndexConfig_DatabaseState new_state) {
  state_ = new_state;

  IndexConfig config;
  config.set_db_directory(index_directory_);
  config.set_ngram_size(ngram_size_);
  config.set_db_parallelism(database_parallelism_);
  config.set_state(state_);

  std::string config_path = index_directory_ + "/config";
  std::ofstream out(config_path.c_str(),
                    std::ofstream::binary |
                    std::ofstream::out |
                    std::ofstream::trunc);
  assert(config.SerializeToOstream(&out));
  out.close();
}

}  // index
