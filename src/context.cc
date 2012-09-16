// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./context.h"

#include <google/protobuf/stubs/common.h>
#include <glog/logging.h>

#include "./mmap.h"
#include "./ngram.h"
#include "./index.pb.h"
#include "./util.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <mutex>

namespace {
std::mutex mut;
std::map<std::string, codesearch::Context *> contexts;

class NGramFrequency {
 public:
  NGramFrequency() = delete;
  NGramFrequency(codesearch::NGram ngram, std::size_t frequency)
      :ngram_(ngram), frequency_(frequency) {}

  bool operator<(const NGramFrequency &other) const {
    return frequency_ < other.frequency_;
  }

  codesearch::NGram ngram() const { return ngram_; }
  std::size_t frequency() const { return frequency_; }

 private:
  codesearch::NGram ngram_;
  std::size_t frequency_;
};
}

namespace codesearch {
Context* GetContext() {
  assert(contexts.size() == 1);
  return contexts.begin()->second;
}

Context::Context(const std::string &index_directory,
                 std::size_t ngram_size,
                 const std::string &vestibule_path,
                 bool create)
    :index_directory_(index_directory), vestibule_(vestibule_path),
     sorted_ngrams_(nullptr), sorted_ngrams_size_(0), ngram_size_(ngram_size) {
  std::string meta_config_path = index_directory + "/meta_config";
  if (create) {
    MetaIndexConfig meta_config;
    meta_config.set_vestibule(vestibule_path);
    meta_config.set_ngram_size(ngram_size);
    std::ofstream of(meta_config_path.c_str(),
                     std::ofstream::out | std::ofstream::binary |
                     std::ofstream::trunc);
    meta_config.SerializeToOstream(&of);
  } else {
    std::ifstream config_file(meta_config_path.c_str(),
                              std::ifstream::binary | std::ifstream::in);
    assert(!config_file.fail());
    MetaIndexConfig meta_config;
    meta_config.ParseFromIstream(&config_file);
    assert(meta_config.ngram_size() == ngram_size);
    vestibule_ = meta_config.vestibule();
  }
}

Context* Context::Acquire(const std::string &db_path,
                          std::size_t ngram_size,
                          const std::string &vestibule_path,
                          bool create) {
  std::lock_guard<std::mutex> guard(mut);
  auto it = contexts.lower_bound(db_path);
  if (it != contexts.end() && it->first == db_path) {
    return it->second;
  }
  Context *ctx = new Context(db_path, ngram_size, vestibule_path, create);
  contexts.insert(it, std::make_pair(db_path, ctx));
  return ctx;
}

std::string Context::FindBestNGram(const std::string &fragment,
                                   std::size_t *offset) {
  InitializeSortedNGrams();
  if (*offset >= sorted_ngrams_size_) {
    return "";
  }

  char *data;
  std::size_t off, extra;
  while (true) {
    if (fragment.size() == 1) {
      data = static_cast<char*>(
          memchr(sorted_ngrams_.get() + *offset,
                 *fragment.begin(),
                 sorted_ngrams_size_ - *offset));
    } else {
      data = static_cast<char*>(
          memmem(sorted_ngrams_.get() + *offset,
                 sorted_ngrams_size_ - *offset,
                 fragment.data(),
                 fragment.size()));
    }

    if (data == nullptr) {
      *offset = sorted_ngrams_size_;
      return "";
    }

    // For this data pointer, figure out where the boundaries for the
    // ngram actually are -- off will be the number of bytes past the
    // start of the sorted ngrams array, and extra will be the number
    // of bytes past the logical ngram. For instance, if off is 100,
    // then the ngram is really composed of the bytes in the range
    // [99, 101], so extra will be 1 here.
    off = data - sorted_ngrams_.get();
    extra = off % NGram::ngram_size;

    // Update the offset pointer to be one past our current position.
    *offset = off + 1;

    // If we're searching for a bigram, we could have split ngrams, in
    // which case we need to keep searching.
    if (extra + fragment.size() > NGram::ngram_size) {
      continue;
    }
    break;
  }

  std::string result(sorted_ngrams_.get() + off - extra, NGram::ngram_size);
  assert(result.find(fragment) != std::string::npos);
  return result;
}

void Context::SortNGrams(std::vector<NGram> *ngrams) {
  InitializeSortedNGrams();
  std::vector<NGramFrequency> frequencies;
  frequencies.reserve(ngrams->size());
  for (const auto &ngram : *ngrams) {
    auto it = ngram_counts_.lower_bound(ngram);
    if (it == ngram_counts_.end()) {
      frequencies.push_back(NGramFrequency(ngram, 0));
    } else {
      frequencies.push_back(NGramFrequency(ngram, it->second));
    }
  }
  std::sort(frequencies.begin(), frequencies.end());
  ngrams->clear();
  for (const auto &freq : frequencies) {
    ngrams->push_back(freq.ngram());
  }
}

void Context::InitializeSortedNGrams() {
  std::lock_guard<std::mutex> guard(mut_);
  if (sorted_ngrams_ != nullptr) {
    // the small ngrams have already been initialized
    return;
  }

  Timer initialization_timer;
  assert(sorted_ngrams_ == nullptr);
  assert(sorted_ngrams_size_ == 0);

  std::string ngram_counts_name = index_directory_ + "/ngram_counts";
  std::ifstream ngram_counts(ngram_counts_name.c_str(),
                             std::ifstream::binary | std::ifstream::in);
  assert(!ngram_counts.fail());
  NGramCounts ngram_counts_proto;
  ngram_counts_proto.ParseFromIstream(&ngram_counts);
  ngram_counts.close();

  sorted_ngrams_size_ = ngram_size_ * ngram_counts_proto.ngram_counts_size();
  sorted_ngrams_ = std::unique_ptr<char[]>(new char[sorted_ngrams_size_]);
  assert(sorted_ngrams_size_ > 0);

  FrozenMapBuilder<NGram, std::size_t> counts_builder;
  char *offset = sorted_ngrams_.get();
  for (const auto &ngram : ngram_counts_proto.ngram_counts()) {
    const std::string &ngram_str = ngram.ngram();
    assert(ngram_str.size() == ngram_size_);
    memcpy(offset, ngram_str.data(), ngram_str.size());
    offset += ngram_str.size();
    counts_builder.insert(NGram(ngram_str), ngram.count());
  }
  ngram_counts_ = counts_builder;
  LOG(INFO) << "initialized sorted ngrams list in " <<
    initialization_timer.elapsed_ms() << " ms\n";
}

void Context::InitializeFileOffsets() {
  std::ifstream ifs(index_directory_ + "/file_start_lines",
                    std::ifstream::binary | std::ifstream::in);
  assert(file_offsets_.empty());
  FileStartLines lines;
  lines.ParseFromIstream(&ifs);

  FrozenMapBuilder<std::uint32_t, std::uint32_t> offsets;
  for (const auto &start_line : lines.start_lines()) {
    offsets.insert(start_line.first_line(), start_line.file_id());
  }
  file_offsets_ = offsets;
}

Context::~Context() {
  UnmapFiles();
  google::protobuf::ShutdownProtobufLibrary();
  std::lock_guard<std::mutex> guard(mut);
  contexts.erase(index_directory_);
}
}
