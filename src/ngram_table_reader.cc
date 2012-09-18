// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./ngram_table_reader.h"

#include <sstream>

namespace {
std::string NameForShard(const std::string &index_directory,
                         std::size_t shard_num) {
  std::stringstream reader_name;
  reader_name << index_directory << "/ngrams/shard_" << shard_num
              << ".sst";
  return reader_name.str();
}
}

namespace codesearch {
NGramTableReader::NGramTableReader(const std::string &index_directory,
                                   std::size_t shard_num,
                                   std::size_t savepoints)
    :reader_(NameForShard(index_directory, shard_num)) {
  name_ = NameForShard(index_directory, shard_num);
  assert(reader_.hdr().index_offset() < 1024);
  FrozenMapBuilder<NGram, std::size_t> builder;
  std::size_t num_keys = reader_.num_keys();
  for (std::size_t i = 0; i < savepoints; i++) {
    std::size_t offset = i * num_keys / savepoints;
    assert(offset < num_keys);
    auto it = reader_.begin() + offset;
    builder.insert(*it, offset);
  }
  savepoints_ = builder;
}

bool NGramTableReader::Find(const NGram &ngram,
                            std::vector<std::uint64_t> *candidates) const {
  auto lo = savepoints_.lower_bound(ngram);
  auto hi = lo;
  if (lo == savepoints_.end()) {
    lo--;
  } else if (lo->first > ngram) {
    assert(lo != savepoints_.begin());
    lo--;
    assert(lo->first <= ngram);
  } else {
    hi++;  // we didn't decrement lo, we increment up
  }
  assert(lo->first <= ngram);

  SSTableReader<NGram>::iterator lower = reader_.begin() + lo->second;
  SSTableReader<NGram>::iterator pos;

  if (hi == savepoints_.end()) {
    // edge case, lo was the last entry in savepoints_
    pos = reader_.lower_bound(lower, reader_.end(), ngram);
  } else {
    assert(hi->first > ngram);
    SSTableReader<NGram>::iterator upper = reader_.begin() + hi->second;
    assert(lower.reader() == upper.reader());
    pos = reader_.lower_bound(lower, upper, ngram);
  }

  if (pos == reader_.end() || *pos != ngram) {
    return false;
  }

  NGramValue val;
  pos.parse_protobuf(&val);
  assert(val.position_ids_size() > 0);
  std::uint64_t posting_val = 0;
  for (const auto &delta : val.position_ids()) {
    posting_val += delta;
    candidates->push_back(posting_val);
  }
  return true;
}
} // namespace codesearch
