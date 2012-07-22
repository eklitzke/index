// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>
//
// A context is a class that lets certain initialization tasks happen,
// and allows for certain resources to be safely shared between
// different threads/objects. For a new binary, a context should be
// created like this, before calling other codesearch functions:
//
// std::unique_ptr<Context> ctx(Context::Acquire("/tmp/index"));
//
// This will ensure the context is properly freed at the end of the
// program. Subsequently, code may safely call the Acquire method to
// get a copy of the context.

#ifndef SRC_CONTEXT_H_
#define SRC_CONTEXT_H_

#include <mutex>
#include <string>
#include <vector>

namespace codesearch {
class Context {
 public:
  static Context* Acquire(const std::string &index_directory);
  ~Context();

  std::string FindBestNGram(const std::string &fragment,
                            std::size_t *offset);

  std::size_t ngram_size() const { return ngram_size_; }

 private:
  Context(const std::string &index_directory);
  std::string index_directory_;
  std::size_t ngram_size_;
  char *sorted_ngrams_;
  std::size_t sorted_ngrams_size_;
  std::mutex mut_;


  void InitializeSmallNGrams();
};
}

#endif
