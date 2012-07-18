// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./ngram_index_writer.h"
#include "./ngram_counter.h"

#include <set>

namespace codesearch {
NGramIndexWriter::NGramIndexWriter(const std::string &index_directory,
                                   std::size_t ngram_size,
                                   std::size_t shard_size)
    :index_writer_(
        index_directory, "ngrams", sizeof(std::uint64_t), shard_size, false),
     files_index_(index_directory, "files", shard_size),
     lines_index_(index_directory, "lines", shard_size),
     ngram_size_(ngram_size), num_vals_(0) {
  index_writer_.Initialize();
}

void NGramIndexWriter::AddFile(const std::string &canonical_name,
                               const std::string &dir_name,
                               const std::string &file_name) {
  // Add the file to the files database
  FileValue file_val;
  file_val.set_directory(dir_name);
  file_val.set_filename(file_name);
  std::uint64_t file_id = files_index_.Add(file_val);

  // Collect all of the lines
  std::map<uint64_t, std::string> positions_map;
  {
    std::ifstream ifs(canonical_name.c_str(), std::ifstream::in);
    std::string line;
    std::size_t linenum = 0;
    while (ifs.good()) {
      std::getline(ifs, line);
      PositionValue val;
      val.set_file_id(file_id);
      val.set_file_offset(ifs.tellg());
      val.set_file_line(++linenum);
      val.set_line(line);

      std::uint64_t line_id = lines_index_.Add(val);
      positions_map.insert(std::make_pair(line_id, line));
    }
    ifs.close();
  }

  // We have all of the lines (in memory!) -- generate a map of type
  // ngram -> [position_id]
  std::map<std::string, std::vector<std::uint64_t> > ngrams_map;
  for (const auto &item : positions_map) {
    const uint64_t position_id = item.first;
    const std::string &line = item.second;
    if (line.size() < ngram_size_) {
      continue;
    }
    std::set<std::string> seen_ngrams;
    for (std::string::size_type i = 0; i <= line.size() - ngram_size_; i++) {
      std::string ngram = line.substr(i, ngram_size_);
      auto pos = seen_ngrams.lower_bound(ngram);
      if (pos == seen_ngrams.end() || *pos != ngram) {
        const auto &map_item = ngrams_map.lower_bound(ngram);
        if (map_item == ngrams_map.end() || map_item->first != ngram) {
          std::vector<std::uint64_t> positions;
          positions.push_back(position_id);
          ngrams_map.insert(map_item, std::make_pair(ngram, positions));
        } else {
          map_item->second.push_back(position_id);
        }
        seen_ngrams.insert(pos, ngram);
     }
    }
  }

  for (const auto &it : ngrams_map) {
    Add(it.first, it.second);
  }
}

void NGramIndexWriter::Add(const std::string &ngram,
                           const std::vector<std::uint64_t> &vals) {
  assert(ngram.size() == ngram_size_);
  const auto it = lists_.lower_bound(ngram);
  if (it != lists_.end() && it->first == ngram) {
    std::vector<std::uint64_t> &ngram_vals = it->second;
    ngram_vals.insert(ngram_vals.end(), vals.begin(), vals.end());
  } else {
    lists_.insert(it, std::make_pair(ngram, vals));
  }
  num_vals_ += vals.size();
  MaybeRotate();
}

std::size_t NGramIndexWriter::EstimateSize() {
  return (2 * sizeof(std::uint64_t) +                 // the SST header
          2 * sizeof(std::uint64_t) * lists_.size() + // the index
          sizeof(std::uint64_t) * num_vals_ / 8);         // guess for data
}

void NGramIndexWriter::MaybeRotate(bool force) {
  if (force || EstimateSize() >= index_writer_.shard_size()) {
    NGramCounter *counter = NGramCounter::Instance();
    for (const auto &it : lists_) {
      NGramValue ngram_val;
      std::size_t val_count = 0;
      {
        std::uint64_t last_val = 0;
        for (const auto v : it.second) {
          assert(!last_val || v > last_val);
          ngram_val.add_position_ids(v - last_val);
          last_val = v;
          val_count++;
        }
      }
      counter->UpdateCount(it.first, val_count);
      index_writer_.Add(it.first, ngram_val);
    }
    index_writer_.Rotate();
    num_vals_ = 0;
    lists_.clear();
  }
}

NGramIndexWriter::~NGramIndexWriter() {
  if (num_vals_ || !lists_.empty()) {
    MaybeRotate(true);
  }
}
}
