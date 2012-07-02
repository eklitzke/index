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
    it->second.insert(it->second.end(), ids.begin(), ids.end());
  } else {
    lists_.insert(it, std::make_pair(ngram, ids));
  }
}

void PostingList::Serialize(leveldb::DB *db) {
  leveldb::Status s;
  std::string key_slice;
  std::string val_slice;

  for (const auto &it : lists_) {
    NGramKey ngram_key;
    ngram_key.set_ngram(it.first);
    ngram_key.SerializeToString(&key_slice);

    NGramValue ngram_val;
    {
      std::uint32_t last_val = 0;
      for (const auto v : it.second) {
        assert(!last_val || v > last_val);
        ngram_val.add_position_ids(v - last_val);
        last_val = v;
      }
    }

    ngram_val.SerializeToString(&val_slice);
    s = db->Put(leveldb::WriteOptions(), key_slice, val_slice);
    assert(s.ok());
  }
}
}
