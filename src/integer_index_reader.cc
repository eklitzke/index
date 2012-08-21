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
  std::uint64_t last_min_key = 0;
  for (std::size_t i = 0; i < index_config.num_shards(); i++) {
    std::string shard_name = (index_directory + "/" + name + "/shard_" +
                              boost::lexical_cast<std::string>(i) + ".sst");
    std::unique_ptr<SSTableReader> shard(new SSTableReader(shard_name));
    std::uint64_t min_key = ToUint64(shard->min_key());
    if (i > 0) {
      assert(min_key > last_min_key);
    }
    last_min_key = min_key;
    shards_.insert(std::make_pair(min_key, std::move(shard)));
  }
}

bool IntegerIndexReader::Find(std::uint64_t needle, std::string *result) const {
  auto shard_it = shards_.lower_bound(needle);
  if (shard_it == shards_.end() || shard_it->first > needle) {
    if (shard_it == shards_.begin()) {
      return false;
    }
    shard_it--;
  }
  assert(shard_it->first <= needle);
  return shard_it->second->Find(needle, result);
}
}
