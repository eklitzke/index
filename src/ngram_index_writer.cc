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
    :files_index_(index_directory, "files", shard_size),
     ngram_size_(ngram_size), max_threads_(max_threads),
     threads_running_(0), file_rotate_(index_directory, shard_size) {}

// Add a file, dispatching to AddFileThread to add the file in its own
// thread. The way this works it there's a condition variable that's
// checking if too many threads are running, and if too many are
// running this method blocks until the condition is false.
//
// Because this launches a new thread for each file (and doesn't
// re-use threads for multiple files) the indexer can potentially do a
// lot of extra work creating short-lived threads. A future
// optimization can be to re-use threads in a thread pool.
std::uint64_t NGramIndexWriter::AddFile(const std::string &canonical_name,
                                        const std::string &dir_name,
                                        const std::string &file_name) {
  // Add the file to the files database.
  //
  // We need to do this *immediately*, before any threading nonsense,
  // to ensure that when the cindex process runs, the file index
  // always contains the files in the same order (internally, cindex
  // will pre-sort all files before calling AddFile in a loop).
  FileValue file_val;
  file_val.set_directory(dir_name);
  file_val.set_filename(file_name);
  file_val.set_lang(FileLanguage(canonical_name));
  std::uint64_t file_id = files_index_.Add(file_val);
  {
    std::unique_lock<std::mutex> lock(threads_running_mut_);
    cond_.wait(lock,
               [=]{return threads_running_ < max_threads_;});
    threads_running_++;
  }
  std::thread t(&NGramIndexWriter::AddFileThread, this,
                file_id, canonical_name);
  t.detach();
  return file_id;
}

void NGramIndexWriter::AddFileThread(std::uint64_t file_id,
                                     const std::string &canonical_name) {
  // Collect all of the lines
  std::vector<PositionValue> positions;
  {
    std::ifstream ifs(canonical_name.c_str(), std::ifstream::in);
    std::string line;
    std::uint64_t linenum = 0;
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
        linenum++;
        continue;
      }
      PositionValue val;
      val.set_file_id(file_id);
      val.set_file_offset(file_offset);
      val.set_file_line(linenum);
      val.set_line(line);

      // N.B. we write *all* valid UTF-8 lines to the index, even
      // those whose length is less than our trigram length. This
      // makes it possible to reconstruct file contents just from the
      // lines index.
      positions.push_back(val);
    }
  }

  // We have all of the lines (in memory!) -- generate a map of type
  // ngram -> [position_id]
  std::map<std::string, std::vector<std::uint64_t> > ngrams_map;
  for (const auto &position : positions) {
    const uint64_t position_id = position.file_line();
    const std::string &line = position.line();
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

  // XXX: verify that copy-elision happens when constructing the
  // PositionsContainer rvalue (that is, that positions and ngrams_map
  // are not copied).
  file_rotate_.AddLines(file_id, PositionsContainer(positions, ngrams_map));

  Notify();
}


// Notify the condition variable thata thread has finished.
void NGramIndexWriter::Notify() {
  std::lock_guard<std::mutex> guard(threads_running_mut_);
  threads_running_--;
  cond_.notify_one();
}

// Wait for all worker threads to terminate.
void NGramIndexWriter::Wait() {
  std::unique_lock<std::mutex> lock(threads_running_mut_);
  cond_.wait(lock, [=]{return threads_running_ == 0;});
}

NGramIndexWriter::~NGramIndexWriter() {
  Wait();
}

NGramIndexWriter::FileRotateThread::FileRotateThread(
    const std::string &index_directory, std::size_t shard_size)
    :index_writer_(
        index_directory, "ngrams", sizeof(std::uint64_t), shard_size, false),
     lines_index_(index_directory, "lines", shard_size),
     last_written_file_(UINT64_MAX) {
  index_writer_.Initialize();
  index_writer_.SetKeyType(IndexConfig_KeyType_STRING);
}

void NGramIndexWriter::FileRotateThread::AddLines(
    std::size_t file_id,
    PositionsContainer positions) {
  std::lock_guard<std::mutex> guard(add_lines_mutex_);
  positions_map_.insert(std::make_pair(file_id, std::move(positions)));
  MaybeRotate();
}

std::size_t NGramIndexWriter::FileRotateThread::EstimateSize(
    std::size_t num_keys, std::size_t num_vals) {
  return (2 * sizeof(std::uint64_t) +                 // the SST header
          2 * sizeof(std::uint64_t) * num_keys +      // the index size
          sizeof(std::uint64_t) * num_vals / 8);      // the data size
}

void NGramIndexWriter::FileRotateThread::MaybeRotate(bool force) {
  // N.B. For the initial rotation, we're relying here on the fact
  // that MAX_UINT64 + 1 == 0.
  std::uint64_t next_file_id = last_written_file_ + 1;

  std::size_t num_keys = 0;
  std::size_t num_vals = 0;
  std::size_t estimated_size = 0;

  auto it = positions_map_.begin();
  while (it != positions_map_.end() &&
         estimated_size < index_writer_.shard_size()) {
    if (it->first == next_file_id) {
      num_keys += it->second.positions_map.size(); // an overstimate!
      num_vals += it->second.positions.size();
      estimated_size = EstimateSize(num_keys, num_vals);
      next_file_id++;
      it++;
    } else {
      break;
    }
  }

  // Everything is already synced to disk!
  if (estimated_size == 0) {
    return;
  }

  // Ensure that at the very end, we're not missing anything.
  if (force) {
    assert(it == positions_map_.end());
  }

  if (force || estimated_size >= index_writer_.shard_size()) {
    // For each file
    std::map<std::string, std::vector<std::uint64_t> > posting_list;
    for (auto map_it = positions_map_.begin(); map_it != it; ++map_it) {
      const std::uint64_t &file_id = map_it->first;
      const std::vector<PositionValue> &positions = map_it->second.positions;
      assert(!positions.empty());

      // Insert all of the PositionValue objects into the "lines" index
      auto positions_it = positions.begin();
      std::uint64_t offset = lines_index_.Add(*positions_it);
      std::uint64_t last_offset = offset;
      for (++positions_it; positions_it != positions.end(); ++positions_it) {
        assert(lines_index_.Add(*positions_it) == ++last_offset);
      }

      // Now we've inserted all of the PositionValue objects. We need
      // to update our in-memory representation of the posting list
      // for this ngram.
      for (const auto &kv : map_it->second.positions_map) {
        std::vector<std::uint64_t> real_positions;
        real_positions.reserve(kv.second.size());
        for (const auto &p : kv.second) {
          real_positions.push_back(p + offset);
        }

        // Find the posting list for the ngram we're looking at
        const std::string &ngram = kv.first;
        auto posting_list_it = posting_list.lower_bound(ngram);
        if (posting_list_it == posting_list.end() ||
            posting_list_it->first != ngram) {
          posting_list.insert(posting_list_it,
                              std::make_pair(ngram, real_positions));
        } else {
          posting_list_it->second.insert(posting_list_it->second.end(),
                                         real_positions.begin(),
                                         real_positions.end());
        }
      }
    }

    // Now it's time to serialize out to disk the "ngrams" index,
    // based on our posting_list. First, as a sanity check, we ensure
    // that the posting list is in fully sorted, which it should be
    // since we're inserting files in sorted order.
    assert(!posting_list.empty());
    for (const auto &kv : posting_list) {
      assert(!kv.second.empty());
      auto pos = kv.second.begin();
      std::uint64_t last_val = *pos;
      for (++pos; pos != kv.second.end(); ++pos) {
        assert(*pos > last_val);
        last_val = *pos;
      }
    }

    // Update our ngrams count.
    NGramCounter::Instance()->UpdateCounts(posting_list);

    // OK, time to serialize the "ngrams" index
    auto posting_it = posting_list.begin();
    std::uint64_t last_val =
        _t
    auto map_it = positions_map_.begin();
    while (map_it !=
    for ()
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
      index_writer_.Add(it.first, ngram_val);
    }
    index_writer_.Rotate();
    num_vals_ = 0;
    lists_.clear();
  }
}

}  // namespace codesearch
