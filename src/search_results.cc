// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./search_results.h"

namespace codesearch {
bool SearchResults::IsFull() {
  std::lock_guard<std::mutex> guard(mutex_);
  return capacity_ && results_.size() >= capacity_;
}

bool SearchResults::AddResult(const std::string &filename, std::size_t line_num,
                              const std::string &line_text) {
  std::lock_guard<std::mutex> guard(mutex_);
  if (!capacity_ || results_.size() < capacity_) {
    if (cur_offset_ < offset_) {
      cur_offset_++;
    }
    if (cur_offset_ >= offset_) {
      SearchResult r;
      r.set_filename(filename);
      r.set_line_num(line_num);
      r.set_line_text(line_text);
      results_.push_back(r);
    }
    return true;
  }
  return false;
}
}
