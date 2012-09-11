// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./bounded_map.h"
#include "./context.h"
#include "./frozen_map.h"
#include "./index.pb.h"
#include "./ngram.h"
#include "./ngram_index_reader.h"
#include "./util.h"

#include <boost/lexical_cast.hpp>
#include <glog/logging.h>

#include <fstream>
#include <functional>
#include <set>
#include <thread>

namespace {
class CondNotifier {
 public:
  CondNotifier(std::mutex &mut, std::condition_variable &cond,
               std::size_t &running_threads)
      :mut_(mut), cond_(cond), running_threads_(running_threads) {}
  ~CondNotifier() {
    std::unique_lock<std::mutex> guard(mut_);
    assert(running_threads_ > 0);
    running_threads_--;
    cond_.notify_one();
  }
 private:
  std::mutex &mut_;
  std::condition_variable &cond_;
  std::size_t &running_threads_;
};

// Choose the concurrency level.
//
// We should always pick at least std::thread::hardware_concurrency(),
// but possibly we would want to pick an even higher level. For
// instance if the hardware concurrency is N, we might want to pick
// 2*N or N+1 or 2*N+1.
//
// In fact, for now we just choose N, i.e. the value of
// std::thread::hardware_concurrency(). The reason is that the most
// pathological queries that can be done are for things like "char" or
// "void", i.e. queries where all of the ngrams have very large
// posting lists. For these queries, we can typically satisfy the
// whole query easily, usually just by querying the very first shard
// -- so we want to keep concurrency low. For queries for more unique
// terms, e.g. "mangosteem jam", we would want a higher concurrency
// level, but since those queries tend to be faster anyway, we choose
// to optimize for the pathological case.
inline std::size_t concurrency() { return std::thread::hardware_concurrency(); }
}

namespace codesearch {
NGramIndexReader::NGramIndexReader(const std::string &index_directory,
                                   SearchStrategy strategy,
                                   std::size_t threads)
    :ctx_(Context::Acquire(index_directory)),
     ngram_size_(ctx_->ngram_size()), files_index_(index_directory, "files"),
     lines_index_(index_directory, "lines"), strategy_(strategy),
     running_threads_(0), parallelism_(threads == 0 ? concurrency() : threads) {
  std::string config_name = index_directory + "/ngrams/config";
  std::ifstream config(config_name.c_str(),
                       std::ifstream::binary | std::ifstream::in);
  assert(!config.fail());
  IndexConfig index_config;
  index_config.ParseFromIstream(&config);
  for (std::size_t i = 0; i < index_config.num_shards(); i++) {
    std::string shard_name = (index_directory + "/ngrams/shard_" +
                              boost::lexical_cast<std::string>(i) + ".sst");
    shards_.push_back(SSTableReader<NGram>(shard_name));
  }
  LOG(INFO) << "initialized NGramIndexReader for directory " <<
      index_directory << " using strategy " << strategy << "\n";
}

void NGramIndexReader::Find(const std::string &query,
                            SearchResults *results) {
  if (query.size() < ngram_size_) {
    FindSmall(query, results);
    return;
  }

  // split the query into its constituent ngrams
  std::vector<NGram> ngrams;
  std::set<NGram> ngrams_set;
  for (std::string::size_type i = 0; i <= query.length() - ngram_size_; i++) {
    ngrams_set.insert(NGram(query.substr(i, ngram_size_)));
  }
  assert(!ngrams_set.empty());
  ngrams.insert(ngrams.begin(), ngrams_set.begin(), ngrams_set.end());
  switch (strategy_.value()) {
    case SearchStrategy::LEXICOGRAPHIC_SORT:
      std::sort(ngrams.begin(), ngrams.end());
      break;
    case SearchStrategy::FREQUENCY_SORT:
      ctx_->SortNGrams(&ngrams);
      break;
    default:
      assert(false);  // not reached
  }

  // An overview of how searching works follows.
  //
  // SEARCH REQUIREMENTS
  // ===================
  //
  // First, there are a couple of properties that we desire in the
  // search results. First, results must be returned in a consistent,
  // repeatable ordering. That means that if you search for "foo", you
  // should always get back the same set of results.
  //
  // Second, if a file has multiple matches we want to collapse the
  // results in the display. The way this works is a search has a
  // parameter for how many files to match, and how many matches to
  // show within a file. So for instance, we might show up to 10
  // files, and up to 10 matches within each of these files. For any
  // file we choose to return in the search result, we want to show
  // the first 10 matches within the file.
  //
  // SEARCH IMPLEMENTATION
  // =====================
  //
  // The search works by spawning one or more threads,
  // repeatedly. Each thread does a full search within a given ngram
  // shard. A property of our indexing is that for any given file, its
  // ngrams are wholly contained within a single ngram index
  // shard. Therefore, one way we can think of the search is that each
  // thread is doing a full search across some list of files. This
  // makes it easy to satisfy our second search requirement above --
  // each thread does its search, and then once it's done that it can
  // merge results from a single file and do limiting.
  //
  // Once a thread has its result set, it puts the results in a shared
  // SearchResults object. This object is thread-safe.
  //
  // The main loop (which is spawning the threads) is notified via a
  // condition variable when each thread returns. The logic it has for
  // when it's OK to stop spawning new threads is that if the results
  // set is ful, and the first N consecutive shards have finished,
  // then we can stop spawning new threads (and waiting for them to
  // join). For instance, let's say we spawn 5 threads. After a while,
  // thread 4 completes, and the result set is not full. Then thread 1
  // completes, and the result set is full. Can we return the results
  // yet? The answer is no! Suppose on another search for the same
  // query, thread 2 returned and then thread 4, and then the results
  // set was full. This would return a different set of results from
  // the time when thread 4 then 1 finished. So in this case, we need
  // to wait to join threads 2 and 3. We do *not* need to wait for
  // thread 5.
  //
  // Once all of the threads have returned, we sort the results, and
  // then trim the result set to only return the first N results. This
  // ensures that we are returning a consistent, repeatable answer for
  // a query.

  auto it = shards_.begin();
  bool spawn_threads = true;
  while (true) {
    // If necessary, create threads. We will always need to create
    // threads here if the shard iterator is not exhausted.
    std::size_t threads_needed = 0;
    if (spawn_threads && it != shards_.end()) {
      std::unique_lock<std::mutex> guard(mut_);
      threads_needed = parallelism_ - running_threads_;
      assert(threads_needed > 0);
      for (std::size_t i = 0; i < threads_needed && it != shards_.end(); i++) {
        running_threads_++;
        std::thread thr(&NGramIndexReader::FindShard, this,
                        std::cref(query), std::cref(ngrams), std::cref(*it),
                        std::ref(results));

        // Detach the thread. The thread will notify our condition
        // variable when its work is done.
        thr.detach();
        ++it;
      }
    }

    // We want to block until this many threads (or fewer) are running.
    std::size_t target_threads = 0;
    if (threads_needed) {
      target_threads = parallelism_ - 1;
    }

    // Wait for a notification
    std::size_t running_threads = WaitForThreads(target_threads);

    // If the results set is full, we can stop now
    if (results->IsFull()) {
      // Don't spawn any more threads; future iterations of this loop
      // will just call WaitForThread();
      spawn_threads = false;
      if (running_threads == 0) {
        // The result set is full, and we have no more running
        // threads, so we can break out of our loop now.
        break;
      }
      // else, there's a simplification of the comment above; we
      // actually wait for all threads to return, to work around what
      // might happend if another request comes in while there are
      // outstanding threads.
    }

    // If we've exhausted all shards, and all threads have joined,
    // then we can also return (and the result set will not be full).
    if ((!threads_needed || it == shards_.end()) && running_threads == 0) {
      break;
    }
  }
}

std::size_t NGramIndexReader::WaitForThreads(std::size_t target) {
  // Wait for a notification
  std::unique_lock<std::mutex> lock(mut_);
  cond_.wait(lock, [=](){ return running_threads_ <= target; });
  LOG(INFO) << "wait for thread completed, running threads = " <<
      running_threads_ << "\n";
  return running_threads_;
}

void NGramIndexReader::FindSmall(const std::string &query,
                                 SearchResults *results) {
  // In a loop, we find the best ngram that contains this query, and
  // then do a search on that ngram. If the result set is not fill, we
  // get the next-best ngram and repeat.
  std::size_t offset = 0;
  while (true) {
    std::string ngram = ctx_->FindBestNGram(query, &offset);
    if (ngram.empty()) {
        break;
    }
    Find(ngram, results);
    if (results->IsFull()) {
      break;
    }
  }
}

void NGramIndexReader::FindShard(const std::string &query,
                                 const std::vector<NGram> &ngrams,
                                 const SSTableReader<NGram> &reader,
                                 SearchResults *results) {
  Timer timer;
  CondNotifier notifier(mut_, cond_, running_threads_);
  SSTableReader<NGram>::iterator lower_bound = reader.begin();
  std::vector<std::uint64_t> candidates;
  std::vector<std::uint64_t> intersection;

  std::vector<NGram>::const_iterator ngrams_iter = ngrams.begin();

  // Get all of the candidates -- that is, all of the lines/positions
  // who have all of the ngrams. To do this we populate the candidates
  // vector, and successively check the remaining ngrams (in
  // lexicographical order), taking the intersection of the candidates.
  if (!GetCandidates(*ngrams_iter, &candidates, reader, &lower_bound)) {
    return;
  }

#if 0
  intersection.reserve(candidates.size());
#endif

  ngrams_iter++;
  for (; ngrams_iter != ngrams.end() && candidates.size() > results->max_keys();
       ++ngrams_iter) {
    std::vector<std::uint64_t> new_candidates;
    if (!GetCandidates(*ngrams_iter, &new_candidates, reader, &lower_bound)) {
      return;
    }

    // Do the actual set intersection, storing the result into the
    // "intersection" vector. Then swap out the contents into our
    // "candidates" vector.
    std::set_intersection(
        candidates.begin(), candidates.end(),
        new_candidates.begin(), new_candidates.end(),
        std::back_inserter(intersection));
    std::swap(candidates, intersection);
    intersection.clear();  // reset for the next loop iteration
  }
  LOG(INFO) << "shard " << reader.shard_name() <<
      " finished SST lookups and set intersections after " <<
      timer.elapsed_us() << " us" <<
      (ngrams_iter != ngrams.end() ? " (and aborted early)" : "") <<
      ", final size is " << candidates.size() << "\n";

  // We are going to construct a map of filename -> [(line num,
  // offset, line)].
  Timer trim_candidates_timer;
  std::size_t lines_added = TrimCandidates(
      query, reader.shard_name(), candidates, results);

  LOG(INFO) << "shard " << reader.shard_name() <<
      " searched query \"" << query << "\" to add " << lines_added <<
      " lines in " << timer.elapsed_us() << " us (trim took " <<
      trim_candidates_timer.elapsed_us() << " us)\n";
}

// Given an ngram, a result list, an "ngram" reader shard, and a
// lower bound, fill in the result list with all of the positions in
// the posting list for that ngram. This is the function that actually
// does the SST lookup.
bool NGramIndexReader::GetCandidates(const NGram &ngram,
                                     std::vector<std::uint64_t> *candidates,
                                     const SSTableReader<NGram> &reader,
                                     SSTableReader<NGram>::iterator *lower_bound) {
  assert(candidates->empty());

  SSTableReader<NGram>::iterator it = reader.lower_bound(*lower_bound, ngram);
  if (strategy_ == SearchStrategy::LEXICOGRAPHIC_SORT) {
    *lower_bound = it;
  }
  if (it == reader.end() || *it != ngram) {
    LOG(INFO) << "did *not* find ngram " << ngram << "\n";
    return false;
  }

  NGramValue ngram_val;
  ngram_val.ParseFromString(it.value());
  assert(ngram_val.position_ids_size() > 0);
  std::uint64_t posting_val = 0;
  for (int i = 0; i < ngram_val.position_ids_size(); i++) {
    std::uint64_t delta = ngram_val.position_ids(i);
    posting_val += delta;
    candidates->push_back(posting_val);
  }
  return true;
}

std::size_t NGramIndexReader::TrimCandidates(
    const std::string &query,
    const std::string &shard_name,
    const std::vector<std::uint64_t> &candidates,
    SearchResults *results) {

  // The candidates vector contains the ids of rows in the "lines"
  // index that are candidates. We need to check each candidate to
  // make sure it really is a match.
  //
  // To do the trimming/sorting for consistent results, we're going to
  // do the full lookup of all of the lines.

  Context *ctx = GetContext();
  const FrozenMap<std::uint32_t, std::uint32_t> &offsets = ctx->file_offsets();
  const bool use_offsets = !offsets.empty();

  std::size_t lines_added = 0;
  std::uint64_t max_file_id = UINT64_MAX;
  for (const auto &candidate : candidates) {
    PositionValue pos;
    std::string value;
    std::uint64_t file_id;

    if (use_offsets) {
      auto it = offsets.lower_bound(candidate);
      if (it == offsets.end()) {
        it--;
        file_id = it->second;
      } else if (it->first == candidate) {
        file_id = it->second;
      } else {
        file_id = it->second - 1;
      }
      if (file_id >= max_file_id) {
        continue;
      }
      assert(lines_index_.Find(candidate, &value));
      pos.ParseFromString(value);
      assert(file_id == pos.file_id());
    } else {
      assert(lines_index_.Find(candidate, &value));
      pos.ParseFromString(value);

      // Check that it's going to be possible to insert with this file
      // id -- no mutexes need to be accessed for this check!
      file_id = pos.file_id();
      if (file_id >= max_file_id) {
        continue;
      }
    }

    // Ensure that the text really matches our query
    if (pos.line().find(query) == std::string::npos) {
      continue;
    }

    files_index_.Find(file_id, &value);
    FileValue fileval;
    fileval.ParseFromString(value);
    FileKey filekey(file_id, fileval.filename());

    BoundedMapInsertionResult status = results->insert(
        filekey, FileResult(pos.file_offset(), pos.file_line()));
    if (status == BoundedMapInsertionResult::INSERT_SUCCESSFUL) {
      lines_added++;
    } else if (status == BoundedMapInsertionResult::KEY_TOO_LARGE) {
      // We failed to insert the file data, because the file_id was
      // too big. Track this file_id, so we can avoid trying to insert
      // with file ids >= this one in the future.
      max_file_id = file_id;
    }
  }
  return lines_added;
}
}  // namespace codesearch
