// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./ngram_index_reader.h"
#include "./index.pb.h"
#include "./util.h"

#include <boost/lexical_cast.hpp>
#include <fstream>
#include <set>
#include <thread>

namespace codesearch {
NGramIndexReader::NGramIndexReader(const std::string &index_directory,
                                   std::size_t ngram_size)
    :ngram_size_(ngram_size), files_index_(index_directory, "files"),
     lines_index_(index_directory, "lines") {
  std::string config_name = index_directory + "/ngrams/config";
  std::ifstream config(config_name.c_str(),
                       std::ifstream::binary | std::ifstream::in);
  assert(!config.fail());
  IndexConfig index_config;
  index_config.ParseFromIstream(&config);
  for (std::size_t i = 0; i < index_config.num_shards(); i++) {
    std::string shard_name = (index_directory + "/ngrams/shard_" +
                              boost::lexical_cast<std::string>(i) + ".sst");
    SSTableReader *shard = new SSTableReader(shard_name);
    shards_.push_back(shard);
  }

  std::string ngram_counts_name = index_directory + "/ngram_counts";
  std::ifstream ngram_counts(ngram_counts_name.c_str(),
                             std::ifstream::binary | std::ifstream::in);
  assert(!ngram_counts.fail());
  NGramCounts ngram_counts_proto;
  ngram_counts_proto.ParseFromIstream(&ngram_counts);
  for (const auto &ngram : ngram_counts_proto.ngram_counts()) {
    sorted_ngrams_.push_back(ngram.ngram());
  }
}

NGramIndexReader::~NGramIndexReader() {
  for (auto &shard : shards_) {
    delete shard;
  }
}

void NGramIndexReader::Find(const std::string &query,
                            SearchResults *results) {
  if (query.size() < ngram_size_) {
    FindSmall(query, results);
    return;
  }

  // split the query into its constituent ngrams
  std::set<std::string> ngrams;
  for (std::string::size_type i = 0;
       i <= query.length() - ngram_size_; i++) {
    ngrams.insert(query.substr(i, ngram_size_));
  }
  assert(!ngrams.empty());

  std::vector<std::thread> threads;
  std::size_t parallelism = std::thread::hardware_concurrency() + 1;
  threads.reserve(parallelism);

  auto it = shards_.begin();
  while (true) {
    // build up a vector of threads to do searches
    for (std::size_t i = 0; i < parallelism; i++) {
      SSTableReader *reader = *it;
      threads.push_back(std::thread(&NGramIndexReader::FindShard, this, query,
                                    ngrams, *reader, results));
      if (++it == shards_.end()) {
        break;
      }
    }
    for (std::size_t i = 0; i < threads.size(); i++) {
      threads[i].join();
    }
    if (results->IsFull() || it == shards_.end()) {
      break;
    }
    threads.clear();
  }
}

void NGramIndexReader::FindSmall(const std::string &query,
                                 SearchResults *results) {
  for (const auto &n : sorted_ngrams_) {
    if (n.find(query) != std::string::npos) {
      Find(n, results);
      if (results->IsFull()) {
        break;
      }
    }
  }
}

void NGramIndexReader::FindShard(const std::string &query,
                                 const std::set<std::string> &ngrams,
                                 const SSTableReader &reader,
                                 SearchResults *results) {
  std::size_t lower_bound = reader.lower_bound();
  std::vector<std::uint64_t> candidates;
  std::vector<std::uint64_t> intersection;

  auto ngrams_iter = ngrams.begin();

  // Get all of the candidates -- that is, all of the lines/positions
  // who have all of the ngrams. To do this we populate the candidates
  // vector, and successively check the remaining ngrams (in
  // lexicographical order), taking the intersection of the candidates.
  if (!GetCandidates(*ngrams_iter, &candidates, reader, &lower_bound)) {
    return;
  }
  ngrams_iter++;
  for (; ngrams_iter != ngrams.end() && !candidates.empty(); ++ngrams_iter) {
    std::vector<std::uint64_t> new_candidates;
    if (!GetCandidates(*ngrams_iter, &new_candidates, reader, &lower_bound)) {
      return;
    }

    intersection.clear();
    intersection.reserve(std::min(candidates.size(), new_candidates.size()));
    auto it = std::set_intersection(
        candidates.begin(), candidates.end(),
        new_candidates.begin(), new_candidates.end(),
        intersection.begin());
#if 0
    candidates.swap(intersection);
#endif
    candidates.clear();
    candidates.reserve(it - intersection.begin());
    candidates.insert(candidates.begin(), intersection.begin(), it);
  }

  // The candidates vector contains the ids of rows in the "lines"
  // index that are candidates. We need to check each candidate to
  // make sure it really is a match.
  for (const auto &p : candidates) {
    PositionValue pos;
    std::string value;
    assert(lines_index_.Find(p, &value));
    pos.ParseFromString(value);

    const std::string &pos_line = pos.line();
    if (pos_line.find(query) != std::string::npos) {
      files_index_.Find(pos.file_id(), &value);
      FileValue fileval;
      fileval.ParseFromString(value);
      if (!results->AddResult(fileval.filename(), pos.file_line(), pos_line)) {
        // the results container is full
        break;
      }
    }
  }
}

bool NGramIndexReader::GetCandidates(const std::string &ngram,
                                     std::vector<std::uint64_t> *candidates,
                                     const SSTableReader &reader,
                                     std::size_t *lower_bound) {
  assert(candidates->empty());
  std::string db_read;
  bool found = reader.FindWithBounds(ngram, &db_read, lower_bound);
  if (!found) {
    return false;
  }

  NGramValue ngram_val;
  ngram_val.ParseFromString(db_read);
  std::uint64_t posting_val = 0;
  for (int i = 0; i < ngram_val.position_ids_size(); i++) {
    std::uint64_t delta = ngram_val.position_ids(i);
    if (delta != 0) {
      posting_val += delta;
      candidates->push_back(posting_val);
    } else {
      std::cerr << "warning, 0 seen in posting list" << std::endl;
    }
  }
  return true;
}
}  // namespace codesearch
