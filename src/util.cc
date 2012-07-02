#include "./util.h"

#include <boost/filesystem.hpp>
#include <leveldb/cache.h>

namespace codesearch {
std::string JoinPath(const std::string &lhs, const std::string &rhs) {
  boost::filesystem::path p(lhs + "/" + rhs);
  //return boost::filesystem::canonical(p).string();
  return p.string();
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
