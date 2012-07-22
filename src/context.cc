// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./context.h"

#include <google/protobuf/stubs/common.h>

#include "./mmap.h"
#include "./index.pb.h"

#include <fstream>
#include <map>
#include <mutex>

namespace {
std::mutex mut;
std::map<std::string, codesearch::Context *> contexts;
}

namespace codesearch {

Context::Context(const std::string &index_directory)
    :index_directory_(index_directory), sorted_ngrams_(nullptr),
     sorted_ngrams_size_(0) {

  // We need to read in the sorted list of ngram counts. Storing this
  // as a std::vector of std::string objects results in a *lot* of
  // memory being wasted, as the strings are generally very small
  // (typically only three bytes). To save memory, we pre-allocate a
  // char[] big enough to hold all of the data.
  std::string config_name = index_directory + "/ngrams/config";
  std::ifstream config_file(config_name.c_str(),
                            std::ifstream::binary | std::ifstream::in);
  assert(!config_file.fail());
  IndexConfig config;
  config.ParseFromIstream(&config_file);
  ngram_size_ = config.ngram_size();
}

Context* Context::Acquire(const std::string &db_path) {
std::lock_guard<std::mutex> guard(mut);
  auto it = contexts.lower_bound(db_path);
  if (it != contexts.end() && it->first == db_path) {
    return it->second;
  }
  Context *ctx = new Context(db_path);
  contexts.insert(it, std::make_pair(db_path, ctx));
  return ctx;
}

std::string Context::FindBestNGram(const std::string &fragment,
                                   std::size_t *offset) {
  {
    std::lock_guard<std::mutex> guard(mut_);
    if (!sorted_ngrams_size_) {
      InitializeSmallNGrams();
      assert(sorted_ngrams_size_);
    }
  }
  char *p = sorted_ngrams_ + *offset;
  while (p < sorted_ngrams_ + sorted_ngrams_size_) {
    void *data = memmem(p, ngram_size_, fragment.data(), fragment.size());
    if (data != nullptr) {
      return std::string(p, ngram_size_);
    }
    *offset += ngram_size_;
    p = sorted_ngrams_ + *offset;
  }
  return "";
}

void Context::InitializeSmallNGrams() {
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
  sorted_ngrams_ = new char[sorted_ngrams_size_];

  char *offset = sorted_ngrams_;
  for (const auto &ngram : ngram_counts_proto.ngram_counts()) {
    const std::string &ngram_str = ngram.ngram();
    assert(ngram_str.size() == ngram_size_);
    memcpy(offset, ngram_str.data(), ngram_str.size());
    offset += ngram_str.size();
  }
}

Context::~Context() {
  UnmapFiles();
  google::protobuf::ShutdownProtobufLibrary();
  std::lock_guard<std::mutex> guard(mut);
  contexts.erase(index_directory_);
  delete[] sorted_ngrams_;
}
}
