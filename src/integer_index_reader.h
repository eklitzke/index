// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_INTEGER_INDEX_READER_H_
#define SRC_INTEGER_INDEX_READER_H_

#include <map>
#include <string>
#include <vector>

#include <google/protobuf/message_lite.h>

#include "./frozen_map.h"
#include "./sstable_reader.h"

namespace codesearch {
class IntegerIndexReader {
 public:
  IntegerIndexReader(const std::string &index_directory,
                     const std::string &name,
                     std::size_t savepoints = 2048);

  // Find a needle. This searches the shards sequentially, and returns
  // early (i.e. on the first shard with the needle). In general, the
  // fact that this is sequential and not parallel shouldn't present
  // any problems, because the index bounds are stored in the SSTable
  // header (which is read into memory when the reader is created),
  // and therefore searching an SSTable without the needle is very,
  // very cheap.
  bool Find(std::uint64_t needle, google::protobuf::MessageLite *msg) const;

 private:
  std::vector<SSTableReader<std::uint64_t> > shards_;
};
}

#endif  // SRC_INTEGER_INDEX_READER_H_
