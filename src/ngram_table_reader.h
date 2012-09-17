// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_NGRAM_TABLE_READER_H_
#define SRC_NGRAM_TABLE_READER_H_

#include <string>
#include <vector>

#include "./frozen_map.h"
#include "./ngram.h"
#include "./sstable_reader.h"

namespace codesearch {
class NGramTableReader {
 public:
  NGramTableReader(const std::string &index_directory,
                   std::size_t shard_num,
                   std::size_t savepoints = 64);

  bool Find(const NGram &ngram,
            std::vector<std::uint64_t> *candidates) const;

  std::string shard_name() const { return reader_.shard_name(); }

 private:
  SSTableReader<NGram> reader_;
  std::string name_;
  //FrozenMap<NGram, SSTableReader<NGram>::iterator> savepoints_;
  FrozenMap<NGram, std::size_t> savepoints_;
};
}

#endif  // SRC_NGRAM_TABLE_READER_H_
