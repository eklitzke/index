// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./search_results.h"
#include "./file_util.h"

namespace codesearch {
bool SearchResults::IsFull() {
  std::lock_guard<std::mutex> guard(mutex_);
  return capacity_ && results_.size() >= capacity_;
}

bool SearchResults::AddResult(const std::string &filename,
                              std::size_t line_num,
                              std::size_t line_offset,
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
      r.set_line_offset(line_offset);
      r.set_line_text(line_text);
      r.set_lang(FileLanguage(filename));  // FIXME, use files index
      results_.push_back(r);
    }
    return true;
  }
  return false;
}

std::vector<SearchResultContext> SearchResults::contextual_results() {
  std::vector<SearchResultContext> results;
  for (const auto &r : results_) {
    SearchResultContext context;
    context.set_filename(r.filename());
    context.set_lang(r.lang());
    std::map<std::size_t, std::string> context_lines = GetFileContext(
        r.filename(), r.line_num(), r.line_offset());
    for (const auto & p : context_lines) {
      SearchResult *sr = context.add_lines();
      sr->set_line_num(p.first);
      sr->set_line_text(p.second);
      if (p.first == r.line_num()) {
        sr->set_is_matched_line(true);
      }
    }
    results.push_back(context);
  }
  return results;
}
}
