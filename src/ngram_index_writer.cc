// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./ngram_index_writer.h"

#include "./file_util.h"
#include "./ngram_counter.h"
#include "./util.h"

#include <set>
#include <thread>

namespace codesearch {
NGramIndexWriter::NGramIndexWriter(const std::string &index_directory,
                                   std::size_t ngram_size,
                                   std::size_t shard_size,
                                   std::size_t max_threads)
    :index_writer_(
        index_directory, "ngrams", sizeof(std::uint64_t), shard_size, false),
     files_index_(index_directory, "files"),
     lines_index_(index_directory, "lines"),
     ngram_size_(ngram_size),
     file_count_(0),
     num_vals_(0),
     index_directory_(index_directory),
     pool_(max_threads) {
  assert(ngram_size == NGram::ngram_size);
  index_writer_.SetKeyType(IndexConfig_KeyType_STRING);
}

// Add a file, dispatching to AddFileThread to add the file in its own
// thread. The way this works it there's a condition variable that's
// checking if too many threads are running, and if too many are
// running this method blocks until the condition is false.
//
// Because this launches a new thread for each file (and doesn't
// re-use threads for multiple files) the indexer can potentially do a
// lot of extra work creating short-lived threads. A future
// optimization can be to re-use threads in a thread pool.
void NGramIndexWriter::AddFile(const std::string &canonical_name,
                               const std::string &dir_name,
                               const std::string &file_name) {
  pool_.Send(std::bind(&NGramIndexWriter::AddFileThread, this,
                       file_count_++, canonical_name, dir_name, file_name));
}

void NGramIndexWriter::AddFileThread(std::size_t file_count,
                                     const std::string &canonical_name,
                                     const std::string &dir_name,
                                     const std::string &file_name) {
  // Add the file to the files database
  FileValue file_val;
  file_val.set_directory(dir_name);
  file_val.set_filename(file_name);
  file_val.set_lang(FileLanguage(canonical_name));

  std::uint64_t file_id;
  {
    IntWait::WaitHandle hdl = files_wait_.Handle(file_count);
    file_id = files_index_.Add(file_val);
  }

  // Collect all of the lines
  std::unordered_map<uint64_t, std::string> positions_map;
  {
    bool first_line = true;
    std::ifstream ifs(canonical_name.c_str(), std::ifstream::in);
    std::string line;
    std::size_t linenum = 0;
    IntWait::WaitHandle hdl = positions_wait_.Handle(file_count);
    while (ifs.good()) {
      std::streampos file_offset = ifs.tellg();
      std::getline(ifs, line);
      if (!IsValidUtf8(line)) {
        // Skip non-utf-8 lines. Anecdotally, these are usually in
        // files that are mostly 7-bit ascii and have one or two lines
        // with weird characters, so it mostly makes sense to index
        // the whole file except for the non-utf-8 lines.
        std::cout << "skipping invalid utf-8 line in " << canonical_name <<
            std::endl;
        continue;
      }
      PositionValue val;
      val.set_file_id(file_id);
      val.set_file_offset(file_offset);
      val.set_file_line(++linenum);
      val.set_line(line);

      // N.B. we write *all* valid UTF-8 lines to the index, even
      // those whose length is less than our trigram length. This
      // makes it possible to reconstruct file contents just from the
      // lines index.
      std::uint64_t line_id = lines_index_.Add(val);
      positions_map.insert({line_id, line});

      // Note the first line in the file
      if (first_line) {
        first_line = false;
        FileStartLine *start_line  = file_start_lines_.add_start_lines();
        start_line->set_file_id(file_id);
        start_line->set_first_line(line_id);
      }
    }
  }

  // We have all of the lines (in memory!) -- generate a map of type
  // ngram -> [position_id]
  std::unordered_map<NGram, std::vector<std::uint64_t> > ngrams_map;
  for (const auto &item : positions_map) {
    const uint64_t position_id = item.first;
    const std::string &line = item.second;
    if (line.size() < ngram_size_) {
      continue;
    }
    std::set<NGram> seen_ngrams;
    for (std::string::size_type i = 0; i <= line.size() - ngram_size_; i++) {
      NGram ngram = NGram(line.substr(i, ngram_size_));
      auto pos = seen_ngrams.lower_bound(ngram);
      if (pos == seen_ngrams.end() || *pos != ngram) {
        const auto &map_item = ngrams_map.find(ngram);
        if (map_item == ngrams_map.end()) {
          std::vector<std::uint64_t> positions;
          positions.push_back(position_id);
          ngrams_map.insert(map_item, {ngram, positions});
        } else {
          map_item->second.push_back(position_id);
        }
        seen_ngrams.insert(pos, ngram);
     }
    }
  }

  {
    IntWait::WaitHandle hdl = ngrams_wait_.Handle(file_count);
    for (const auto &it : ngrams_map) {
      Add(it.first, it.second);
    }
    MaybeRotate();
  }
}

void NGramIndexWriter::Add(const NGram &ngram,
                           const std::vector<std::uint64_t> &vals) {
  const auto it = lists_.lower_bound(ngram);
  if (it != lists_.end() && it->first == ngram) {
    std::vector<std::uint64_t> &ngram_vals = it->second;
    ngram_vals.insert(ngram_vals.end(), vals.begin(), vals.end());
  } else {
    lists_.insert(it, {ngram, vals});
  }
  num_vals_ += vals.size();
}

std::size_t NGramIndexWriter::EstimateSize() {
  return (2 * sizeof(std::uint64_t) +                 // the SST header
          2 * sizeof(std::uint64_t) * lists_.size() + // the index
          sizeof(std::uint64_t) * num_vals_ / 6);     // guess for data
}

void NGramIndexWriter::MaybeRotate(bool force) {
  if (force || EstimateSize() >= index_writer_.shard_size()) {
    NGramCounter *counter = NGramCounter::Instance();
    for (auto &it : lists_) {
      NGramValue ngram_val;
      std::size_t val_count = 0;

      // Because of the loose locking we have, position ids can be
      // added out of order. We need to re-order them before we add
      // them into the posting list.
      std::sort(it.second.begin(), it.second.end());
      std::uint64_t last_val = 0;
      for (const auto &v : it.second) {
        assert(!last_val || v > last_val);
        ngram_val.add_position_ids(v - last_val);
        last_val = v;
        val_count++;
      }
      counter->UpdateCount(it.first, val_count);
      index_writer_.Add(it.first.string(), ngram_val);
    }
    index_writer_.Rotate();
    num_vals_ = 0;
    lists_.clear();
  }
}

NGramIndexWriter::~NGramIndexWriter() {
  pool_.Wait();
  if (num_vals_ || !lists_.empty()) {
    MaybeRotate(true);
  }
  std::ofstream ofs(index_directory_ + "/file_start_lines",
                    std::ofstream::binary | std::ofstream::out);
  file_start_lines_.SerializeToOstream(&ofs);
}
}
