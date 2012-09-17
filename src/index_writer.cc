// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./index_writer.h"
#include "./util.h"

#include <boost/lexical_cast.hpp>

#include <iostream>
#include <fstream>
#include <map>
#include <set>

#include "./index.pb.h"

namespace codesearch {
IndexWriter::IndexWriter(const std::string &index_directory,
                         const std::string &name,
                         std::size_t key_size,
                         std::size_t shard_size,
                         bool auto_rotate)
    :index_directory_(index_directory), name_(name), key_size_(key_size),
     shard_size_(shard_size), auto_rotate_(auto_rotate), shard_num_(0),
     key_type_(IndexConfig_KeyType_NUMERIC),
     state_(IndexConfig_DatabaseState_EMPTY), sstable_(nullptr) {
  boost::filesystem::path p(GetPathName(""));
  assert(!boost::filesystem::is_directory(p));
  boost::filesystem::create_directories(p);

  WriteStatus(IndexConfig_DatabaseState_EMPTY);
}

IndexWriter::~IndexWriter() {
  if (sstable_ != nullptr) {
    sstable_->Merge();
    delete sstable_;
  }
  WriteStatus(IndexConfig_DatabaseState_COMPLETE);
}

std::string IndexWriter::GetPathName(const std::string &name) {
  if (!name.empty()) {
    return index_directory_ + "/" + name_ + "/" + name;
  } else {
    return index_directory_ + "/" + name_;
  }
}

void IndexWriter::EnsureSSTable() {
  if (sstable_ == nullptr) {
    std::string name = GetPathName(
        "shard_" + boost::lexical_cast<std::string>(shard_num_));
    sstable_ = new SSTableWriter(name, key_size_);
  }
}

void IndexWriter::Rotate() {
  assert(sstable_ != nullptr);
  sstable_->Merge();
  delete sstable_;
  sstable_ = nullptr;
  shard_num_++;
}

void IndexWriter::WriteStatus(IndexConfig_DatabaseState new_state) {
  state_ = new_state;

  IndexConfig config;
  config.set_shard_size(shard_size_);
  std::uint64_t num_shards = shard_num_ + 1;
  if (sstable_ == nullptr) {
    num_shards--;
  }
  config.set_num_shards(num_shards);
  config.set_state(state_);
  config.set_key_type(key_type_);

  std::string config_path = GetPathName("config");
  std::ofstream out(config_path.c_str(),
                    std::ofstream::binary |
                    std::ofstream::out |
                    std::ofstream::trunc);
  assert(config.SerializeToOstream(&out));
  out.close();
}

}  // index
