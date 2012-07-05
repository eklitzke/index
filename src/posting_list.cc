// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./posting_list.h"
#include "./index.pb.h"
#include "./util.h"

#include <iostream>
#include <fstream>
#include <map>

namespace codesearch {
void PostingList::UpdateNGram(const std::string &ngram,
                              const std::vector<std::uint64_t> &ids) {
  const auto it = lists_.lower_bound(ngram);
  if (it != lists_.end() && it->first == ngram) {
    std::vector<std::uint64_t> &ngram_ids = it->second;
    ngram_ids.insert(ngram_ids.end(), ids.begin(), ids.end());
  } else {
    lists_.insert(it, std::make_pair(ngram, ids));
  }
}

void PostingList::Serialize(SSTableWriter *db) {
  for (const auto &it : lists_) {
    NGramValue ngram_val;
    {
      std::uint32_t last_val = 0;
      for (const auto v : it.second) {
        assert(!last_val || v > last_val);
        ngram_val.add_position_ids(v - last_val);
        last_val = v;
      }
    }
    db->Add(it.first, ngram_val);
  }
}
}
