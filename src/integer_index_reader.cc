// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./integer_index_reader.h"
#include "./index.pb.h"
#include "./util.h"

#include <boost/lexical_cast.hpp>
#include <fstream>

namespace codesearch {
IntegerIndexReader::IntegerIndexReader(const std::string &index_directory,
                                       const std::string &name,
                                       std::size_t savepoints) {
  std::string config_name = index_directory + "/" + name + "/config";
  std::ifstream config(config_name.c_str(),
                       std::ifstream::binary | std::ifstream::in);
  IndexConfig index_config;
  index_config.ParseFromIstream(&config);
  assert(index_config.num_shards() > 0);
  for (std::size_t i = 0; i < index_config.num_shards(); i++) {
    std::string shard_name = (index_directory + "/" + name + "/shard_" +
                              boost::lexical_cast<std::string>(i) + ".sst");
    shards_.push_back(SSTableReader<std::uint64_t>(shard_name));
  }
}

bool IntegerIndexReader::Find(std::uint64_t needle,
                              google::protobuf::MessageLite *msg) const {
  for (const auto &shard : shards_) {
    std::uint64_t min_key = ToUint64(shard.hdr().min_value());
    std::uint64_t max_key = ToUint64(shard.hdr().max_value());
    if (needle >= min_key && needle <= max_key) {
      std::uint64_t delta = needle - min_key;
      SSTableReader<std::uint64_t>::iterator pos = shard.begin() + delta;
      assert(*pos == needle);
      pos.parse_protobuf(msg);
      return true;
    }
  }
  assert(false);  // should never happen
  return false;
}
}
