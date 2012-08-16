// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./search_results.h"

#include <algorithm>

#include "./context.h"
#include "./file_util.h"

namespace codesearch {
bool SearchResults::IsFull() {
  std::lock_guard<std::mutex> guard(mutex_);
  return UnlockedIsFull();
}

void SearchResults::Trim() {
  assert(capacity_);
  const std::size_t max_size = capacity_ + offset_;
  if (results_.size() > max_size) {
    std::map<std::string, std::vector<FileResult> > results;
    std::size_t i = 0;
    for (const auto & p : results_) {
      results.insert(p);
      if (++i >= max_size) {
        break;
      }
    }
    std::swap(results_, results);
  }
  assert(results_.size() <= max_size);
}

bool SearchResults::AddFileResult(const std::string &filename,
                                  const std::vector<FileResult> &lines) {
  std::lock_guard<std::mutex> guard(mutex_);
  auto pos = results_.lower_bound(filename);
  if (pos != results_.end() && pos->first == filename) {
    // This will happen sometimes with small indexes, when we do a
    // search for a term smaller than a trigram. We need to add in the
    // new lines to the result.
    std::vector<FileResult> &file_lines = pos->second;
    file_lines.insert(file_lines.end(), lines.begin(), lines.end());
    std::sort(file_lines.begin(), file_lines.end());
    if (file_lines.size() > within_file_limit_) {
      file_lines.erase(file_lines.begin() + within_file_limit_,
                       file_lines.end());
    }
    assert(file_lines.size() <= within_file_limit_);
  } else {
    // The usual case, the file has not yet been added to the search results
    assert(lines.size() <= within_file_limit_);
    results_.insert(pos, std::make_pair(filename, lines));
  }
  return UnlockedIsFull();
}

std::vector<SearchResultContext> SearchResults::contextual_results() {
  std::lock_guard<std::mutex> guard(mutex_);
  std::vector<SearchResultContext> results;

  const std::string &vestibule = GetContext()->vestibule();

  std::size_t offset_counter = 0;
  for (const auto &kv : results_) {
    if (offset_counter < offset_) {
      offset_counter++;
      continue;
    }
    SearchResultContext context;
    context.set_filename(kv.first);

    // We have to get all of the lines out of the file... we're going
    // to make a map of line_num -> (is_match, line_text) and then
    // fill in the context map with that.
    std::map<std::size_t, std::pair<bool, std::string> > context_lines;
    std::ifstream ifs(vestibule + "/" + kv.first,
                      std::ifstream::in | std::ifstream::binary);
    assert(!ifs.fail());

    // For each line in the matched lines for this file...
    for (const auto &line : kv.second) {
      std::map<std::size_t, std::string> inner_context = GetFileContext(
          &ifs, line.line_number, line.offset);
      for (const auto &context_kv : inner_context) {
        bool is_matched = context_kv.first == line.line_number;
        auto pos = context_lines.lower_bound(context_kv.first);
        if (pos == context_lines.end() || pos->first != context_kv.first) {
          // this line num / line is not in the context_lines map
          context_lines.insert(
              pos, std::make_pair(context_kv.first,
                                  std::make_pair(
                                      is_matched, context_kv.second)));
        } else {
          // the line num is in the map; just update is_matched field
          pos->second.first = is_matched;
        }
      }
    }

    // Great, now we're ready to fill out a SearchResult structure
    for (const auto &line : context_lines) {
      SearchResult *sr = context.add_lines();
      sr->set_line_num(line.first);
      sr->set_is_matched_line(line.second.first);
      sr->set_line_text(line.second.second);
    }

    results.push_back(context);
  }
  return results;
}
}
