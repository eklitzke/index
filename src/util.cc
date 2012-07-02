#include "./util.h"

#include <leveldb/cache.h>
#include <sstream>

namespace codesearch {

std::string ConstructShardPath(const std::string &index_directory,
                               const std::string &name,
                               std::uint32_t shard_num) {
  std::stringstream ss (std::stringstream::in | std::stringstream::out);
  ss << index_directory << "/" << name << "_" << shard_num;
  return ss.str();
}

leveldb::Options DefaultOptions() {
  leveldb::Options opts;
  opts.compression = leveldb::kSnappyCompression;
  opts.write_buffer_size = 100 << 20;
  opts.block_size = 1 << 20;
  opts.block_cache = leveldb::NewLRUCache(100 << 20);  // 100MB
  return opts;
}
}
