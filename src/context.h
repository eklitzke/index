// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>
//
// A context is a class that lets certain initialization tasks happen,
// and allows for certain resources to be safely shared between
// different threads/objects. For a new binary, a context should be
// created like this, before calling other codesearch functions:
//
// std::unique_ptr<Context> ctx(Context::Acquire("/var/codesearch"));
//
// This will ensure the context is properly freed at the end of the
// program. Subsequently, code may safely call the Acquire method to
// get a copy of the context.

#ifndef SRC_CONTEXT_H_
#define SRC_CONTEXT_H_

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "./ngram.h"

namespace codesearch {
class Context {
 public:
  // If create is true, a meta_config file will be created
  static Context* Acquire(const std::string &index_directory,
                          std::size_t ngram_size = 3,
                          const std::string &vestibule_path = "",
                          bool create = false);
  ~Context();

  std::string FindBestNGram(const std::string &fragment,
                            std::size_t *offset);

  std::size_t ngram_size() const { return ngram_size_; }
  std::string vestibule() const { return vestibule_; }

  void SortNGrams(std::vector<NGram> *ngrams);

  // Initialize the list of small ngrams -- normally this method will
  // be called on demand (that is, the first time a query is done for
  // a small ngram).
  void InitializeSortedNGrams();

  void InitializeFileOffsets();

  const std::map<std::uint32_t, std::uint32_t> &file_offsets() const {
    return file_offsets_;
  }

 private:
  Context(const std::string &index_directory,
          std::size_t ngram_size,
          const std::string &vestibule_path,
          bool create);
  std::string index_directory_;
  std::string vestibule_;

  std::unique_ptr<char[]> sorted_ngrams_;
  std::size_t sorted_ngrams_size_;

  // maps ngram to count
  std::map<NGram, std::size_t> ngram_counts_;

  // a "map" of file_id -> id of the first line in the file
  std::map<std::uint32_t, std::uint32_t> file_offsets_;

  const std::size_t ngram_size_;
  std::mutex mut_;
};

// This allows getting the current context, assuming that only one has
// been created (if zero or 2+ have been created, this will trigger an
// assertion failure).
Context* GetContext();
}

#endif
