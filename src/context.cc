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
  char *p = sorted_ngrams_.get() + *offset;
  while (p < sorted_ngrams_.get() + sorted_ngrams_size_) {
    void *data = memmem(p, ngram_size_, fragment.data(), fragment.size());
    *offset += ngram_size_;
    if (data != nullptr) {
      return std::string(p, ngram_size_);
    }
    p = sorted_ngrams_.get() + *offset;
  }
  return "";
}

void Context::SortNGrams(std::vector<NGram> *ngrams) {
  InitializeSortedNGrams();
  std::vector<NGramFrequency> frequencies;
  frequencies.reserve(ngrams->size());
  for (const auto &ngram : *ngrams) {
    auto it = ngram_counts_.find(ngram);
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

  char *offset = sorted_ngrams_.get();
  for (const auto &ngram : ngram_counts_proto.ngram_counts()) {
    const std::string &ngram_str = ngram.ngram();
    assert(ngram_str.size() == ngram_size_);
    memcpy(offset, ngram_str.data(), ngram_str.size());
    offset += ngram_str.size();
    ngram_counts_.insert(std::make_pair(NGram(ngram_str), ngram.count()));
  }
  LOG(INFO) << "initialized sorted ngrams list in " <<
    initialization_timer.elapsed_ms() << " ms\n";
}

void Context::InitializeFileOffsets() {
  std::ifstream ifs(index_directory_ + "/file_start_lines",
                    std::ifstream::binary | std::ifstream::in);
  assert(file_offsets_.empty());
  FileStartLines lines;
  lines.ParseFromIstream(&ifs);

  for (const auto &start_line : lines.start_lines()) {
    file_offsets_.insert(std::make_pair(start_line.first_line(),
                                        start_line.file_id()));
  }
}

Context::~Context() {
  UnmapFiles();
  google::protobuf::ShutdownProtobufLibrary();
  std::lock_guard<std::mutex> guard(mut);
  contexts.erase(index_directory_);
}
}
