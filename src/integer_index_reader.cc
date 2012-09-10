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
    shards_.push_back(SSTableReader(shard_name));
  }

  // Now, we slice up all of the "lines" (or whatever the integer
  // values are) into a map of lineth percentile line -> reader. For
  // instance, say we have two shards, each with 1000 "lines"", and
  // savepoints = 100. Then at the end of this, we'll basically have a
  // map (i.e. savepoints_) like:
  //
  //  0    ->  shard_0.begin()
  //  20   ->  shard_0.begin() + 20
  //  40   ->  shard_0.begin() + 40
  //  ...
  //  980  ->  shard_0.begin() + 980
  //  1000 ->  shard_1.begin()
  //  1020 ->  shard_2.begin() + 20
  //  ...
  //
  // The Find() method will then use this savepoints_ map to optimize
  // lookups, see the comment there to see how that works.
  std::uint64_t total_lines = 0;
  std::map<std::uint64_t, const SSTableReader*> line_offset_to_shard;
  for (const auto &shard : shards_) {
    line_offset_to_shard.insert(std::make_pair(total_lines, &shard));
    total_lines += shard.num_keys();
  }
  savepoints = std::min(savepoints, total_lines);
  for (std::size_t i = 0; i < savepoints; i++) {
    std::uint64_t absolute_line = i * total_lines / savepoints;
    auto s = line_offset_to_shard.lower_bound(absolute_line);
    if (s == line_offset_to_shard.end() || s->first > absolute_line) {
      assert(s != line_offset_to_shard.begin());
      s--;
      assert(s->first <= absolute_line);
    }

    std::size_t line_offset = s->first;
    const SSTableReader *reader = s->second;

    auto it = reader->begin() + (absolute_line - line_offset);
    savepoints_.insert(
        std::make_pair(absolute_line, std::make_pair(it, reader)));
  }
}

bool IntegerIndexReader::Find(std::uint64_t needle, std::string *result) const {
  // Continuing from our example above, we want to use the savepoints_
  // map to optimize our lookups. For instance, let's say we're
  // searching for line 333 in our example from above. Using
  // savepoints_.lower_bound(), we'll fine the entry that's like
  //
  //  320 -> shard_0.begin() + 320
  //
  // and then the following entry that's like
  //
  //  340 -> shard_0.begin() + 340
  //
  // We'll then pass these two iterators to the
  // SSTableReader::lower_bound method which will do an optimized
  // search between these two offsets. There are some edge cases we
  // need to handle here for what happens if the following item in the
  // savepoints_ map is for another shard, or the lower bound is the
  // last item in the map, etc. which accounts for most of the
  // complexity below.

  auto it = savepoints_.lower_bound(needle);
  if (it == savepoints_.end() || it->first > needle) {
    assert(it != savepoints_.begin());
    it--;
    assert(it->first <= needle);
  }
  assert(it->first <= needle);
  const SSTableReader *reader = it->second.second;
  SSTableReader::iterator lower = it->second.first;

  SSTableReader::iterator pos;
  std::string val = Uint64ToString(needle);
  reader->PadNeedle(&val);

  auto up = it;
  up++;
  if (up == savepoints_.end()) {
    pos = reader->lower_bound(lower, reader->end(), val);
  } else {
    SSTableReader::iterator upper = up->second.first;
    if (lower.reader() == upper.reader()) {
      pos = reader->lower_bound(lower, upper, val);
    } else {
      pos = reader->lower_bound(lower, reader->end(), val);
    }
  }

  if (*pos == val) {
    *result = pos.value();
    return true;
  }
  return false;
}
}
