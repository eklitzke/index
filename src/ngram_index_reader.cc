// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./bounded_map.h"
#include "./context.h"
#include "./frozen_map.h"
#include "./index.pb.h"
#include "./ngram.h"
#include "./ngram_index_reader.h"
#include "./queue.h"
#include "./util.h"

#include <glog/logging.h>

#include <fstream>
#include <functional>
#include <set>
#include <thread>

namespace codesearch {

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

struct QueryRequest {
  QueryRequest(const std::string &q,
               const std::vector<NGram> &n,
               SearchResults *r)
      :query(q), ngrams(n), results(r) {}

  const std::string &query;
  const std::vector<NGram> &ngrams;
  SearchResults *results;
};

NGramReaderWorker::NGramReaderWorker(Queue<NGramReaderWorker*> *responses,
                                     Queue<NGramReaderWorker*> *terminate_responses,
                                     NGramIndexReader *index_reader)
    :responses_(responses), terminate_responses_(terminate_responses),
     index_reader_(index_reader), keep_going_(true), req_(nullptr),
     shard_(nullptr) {}

void NGramReaderWorker::Run() {
  std::unique_lock<std::mutex> lock(mut_);
  while (true) {
    cond_.wait(lock, [=](){ return !keep_going_ || req_ != nullptr; });
    if (!keep_going_) {
      break;
    }

    FindShard();
    req_ = nullptr;
    shard_ = nullptr;
    responses_->push(this);
  }
  terminate_responses_->push(this);
}

void NGramReaderWorker::Stop() {
  std::unique_lock<std::mutex> lock(mut_);
  keep_going_ = false;
  cond_.notify_all();
}

void NGramReaderWorker::SendRequest(const QueryRequest *req,
                                    const NGramTableReader *shard) {
  std::unique_lock<std::mutex> lock(mut_);
  cond_.wait(lock, [=]() { return req_ == nullptr; });
  assert(req_ == nullptr);
  req_ = req;
  shard_ = shard;
  cond_.notify_all();
}

void NGramReaderWorker::FindShard() {
  Timer timer;
  std::vector<std::uint64_t> candidates;
  std::vector<std::uint64_t> intersection;

  NGramValue ngram_val;
  std::vector<NGram>::const_iterator ngrams_iter = req_->ngrams.cbegin();

  // Get all of the candidates -- that is, all of the lines/positions
  // who have all of the ngrams. To do this we populate the candidates
  // vector, and successively check the remaining ngrams (in
  // lexicographical order), taking the intersection of the
  // candidates.
  if (!shard_->Find(*ngrams_iter, &candidates)) {
    return;
  }

#if 0
  intersection.reserve(candidates.size());
#endif


  ngrams_iter++;
  for (; ngrams_iter != req_->ngrams.end() &&
           candidates.size() > req_->results->max_keys();
       ++ngrams_iter) {
    std::vector<std::uint64_t> new_candidates;
    if (!shard_->Find(*ngrams_iter, &new_candidates)) {
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
  LOG(INFO) << "shard " << shard_->shard_name() <<
      " finished SST lookups and set intersections after " <<
      timer.elapsed_us() << " us" <<
      (ngrams_iter != req_->ngrams.end() ? " (and aborted early)" : "") <<
      ", final size is " << candidates.size() << "\n";

  // We are going to construct a map of filename -> [(line num,
  // offset, line)].
  Timer trim_candidates_timer;
  std::size_t lines_added = TrimCandidates(candidates);

  LOG(INFO) << "shard " << shard_->shard_name() <<
      " searched query \"" << req_->query << "\" to add " << lines_added <<
      " lines in " << timer.elapsed_us() << " us (trim took " <<
      trim_candidates_timer.elapsed_us() << " us)\n";
}

std::size_t NGramReaderWorker::TrimCandidates(
    const std::vector<std::uint64_t> &candidates) {
  // The candidates vector contains the ids of rows in the "lines"
  // index that are candidates. We need to check each candidate to
  // make sure it really is a match.
  //
  // To do the trimming/sorting for consistent results, we're going to
  // do the full lookup of all of the lines.
  const FrozenMap<std::uint32_t, std::uint32_t> &offsets =\
      index_reader_->ctx_->file_offsets();
  const bool use_offsets = !offsets.empty();

  std::size_t lines_added = 0;
  std::uint64_t max_file_id = UINT64_MAX;
  PositionValue pos;
  for (const auto &candidate : candidates) {
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
      assert(index_reader_->lines_index_.Find(candidate, &pos));
      assert(file_id == pos.file_id());
    } else {
      assert(index_reader_->lines_index_.Find(candidate, &pos));

      // Check that it's going to be possible to insert with this file
      // id -- no mutexes need to be accessed for this check!
      file_id = pos.file_id();
      if (file_id >= max_file_id) {
        continue;
      }
    }

    // Ensure that the text really matches our query
    if (pos.line().find(req_->query) == std::string::npos) {
      continue;
    }

    FileValue fileval;
    index_reader_->files_index_.Find(file_id, &fileval);
    FileKey filekey(file_id, fileval.filename());

    BoundedMapInsertionResult status = req_->results->insert(
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

NGramIndexReader::NGramIndexReader(const std::string &index_directory,
                                   std::size_t threads)
    :ctx_(Context::Acquire(index_directory)),
     files_index_(index_directory, "files"),
     lines_index_(index_directory, "lines"),
     parallelism_(threads == 0 ? concurrency() : threads) {
  std::string config_name = index_directory + "/ngrams/config";
  std::ifstream config(config_name.c_str(),
                       std::ifstream::binary | std::ifstream::in);
  assert(!config.fail());
  IndexConfig index_config;
  index_config.ParseFromIstream(&config);
  for (std::size_t i = 0; i < index_config.num_shards(); i++) {
    shards_.emplace_back(index_directory, i);
  }

  for (std::size_t i = 0; i < parallelism_; i++) {
    NGramReaderWorker *worker = new NGramReaderWorker(
        &response_queue_, &terminate_response_queue_, this);
    std::thread thr(&NGramReaderWorker::Run, worker);
    thr.detach();
    free_workers_.push_back(worker);
  }

  LOG(INFO) << "initialized NGramIndexReader for directory " <<
      index_directory << "\n";
}

NGramIndexReader::~NGramIndexReader() {
  // make sure all of the workers are done doing their queries
  while (free_workers_.size() < parallelism_) {
    free_workers_.push_back(response_queue_.pop());
  }

  // ask all of the workers to stop
  for (auto &worker : free_workers_) {
    worker->Stop();
  }

  // don't delete the worker objects until they actually have stopped
  for (std::size_t i = 0; i < parallelism_; i++) {
    delete terminate_response_queue_.pop();
  }
}

void NGramIndexReader::Find(const std::string &query,
                            SearchResults *results) {
  if (query.size() < NGram::ngram_size) {
    FindSmall(query, results);
    return;
  }

  // split the query into its constituent ngrams
  std::vector<NGram> ngrams;
  std::set<NGram> ngrams_set;
  for (std::string::size_type i = 0;
       i <= query.length() - NGram::ngram_size; i++) {
    ngrams_set.insert(NGram(query.substr(i, NGram::ngram_size)));
  }
  assert(!ngrams_set.empty());
  ngrams.insert(ngrams.begin(), ngrams_set.begin(), ngrams_set.end());
  ctx_->SortNGrams(&ngrams);
  FindNGrams(query, ngrams, results);
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
    std::vector<NGram> ngrams{NGram(ngram)};
    FindNGrams(query, ngrams, results);
    if (results->IsFull()) {
      break;
    }
  }
}

void NGramIndexReader::FindNGrams(const std::string &query,
                                  const std::vector<NGram> ngrams,
                                  SearchResults *results) {

  Timer timer;
  QueryRequest req(query, ngrams, results);

  for (const auto &shard : shards_) {
    if (free_workers_.empty()) {
      free_workers_.push_back(response_queue_.pop());
    }
    if (results->IsFull()) {
      break;
    }
    NGramReaderWorker *worker = free_workers_.back();
    free_workers_.pop_back();
    worker->SendRequest(&req, &shard);
  }

  // wait for all of the pending workers to finish
  while (free_workers_.size() < parallelism_) {
    free_workers_.push_back(response_queue_.pop());
  }
  LOG(INFO) << "done with FindNGrams() after " << timer.elapsed_us() << " us\n";
}

}  // namespace codesearch
