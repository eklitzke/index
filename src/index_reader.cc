// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./index_reader.h"
#include "./index.pb.h"
#include "./util.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <set>
#include <thread>

namespace codesearch {
bool IndexReader::Verify() {
  IndexConfig config;
  std::string config_path = index_directory_ + "/config";
  std::ifstream infile(config_path.c_str(),
                       std::ifstream::binary | std::ifstream::in);
  config.ParseFromIstream(&infile);
  infile.close();

  ngram_size_ = config.ngram_size();
  database_parallelism_ = config.db_parallelism();
  return (config.state() == IndexConfig_DatabaseState_COMPLETE);
}

bool IndexReader::Initialize() {
  if (!Verify()) {
    return false;
  }
  shards_.reserve(database_parallelism_);
  for (std::uint32_t i = 0; i < database_parallelism_; i++) {
    ShardReader *shard = new ShardReader(index_directory_, ngram_size_, i);
    if (!shard->Initialize()) {
      return false;
    }
    shards_.push_back(shard);
  }
  return true;
}

bool IndexReader::Search(const std::string &query,
                         SearchResults *results) {
  results->Reset();
#ifdef USE_THREADS
  std::vector<std::thread> threads;
  for (const auto &shard : shards_) {
    threads.push_back(std::thread(&ShardReader::Search, shard, query));
  }

  std::size_t i = 0;
  for (const auto & shard : shards_) {
    threads[i++].join();
    results->Extend(shard->results());
  }
#else
  for (const auto & shard : shards_) {
    shard->Search(query);
    results->Extend(shard->results());
  }
#endif
  return true;
}

IndexReader::~IndexReader() {
  for (auto &shard : shards_) {
    delete shard;
  }
}
}
