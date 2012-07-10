// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./integer_index_reader.h"
#include "./index.pb.h"
#include "./util.h"

#include <boost/lexical_cast.hpp>
#include <fstream>

namespace codesearch {
IntegerIndexReader::IntegerIndexReader(const std::string &index_directory,
                         const std::string &name) {
  std::string config_name = index_directory + "/" + name + "/config";
  std::ifstream config(config_name.c_str(),
                       std::ifstream::binary | std::ifstream::in);
  IndexConfig index_config;
  index_config.ParseFromIstream(&config);
  assert(index_config.num_shards() > 0);
  for (std::size_t i = 0; i < index_config.num_shards(); i++) {
    std::string shard_name = (index_directory + "/" + name + "/shard_" +
                              boost::lexical_cast<std::string>(i) + ".sst");
    SSTableReader *shard = new SSTableReader(shard_name);
    shards_.push_back(shard);
  }
}

IntegerIndexReader::~IntegerIndexReader() {
  for (auto &shard : shards_) {
    delete shard;
  }
}

bool IntegerIndexReader::Find(std::uint64_t needle, std::string *result) const {
  for (const auto &shard : shards_) {
    if (shard->Find(needle, result)) {
      return true;
    }
  }
  return false;
}
}
