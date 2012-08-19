// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./search_results.h"

#include <glog/logging.h>

#include <algorithm>
#include <iostream>

#include "./context.h"
#include "./file_util.h"

namespace codesearch {

std::vector<SearchResultContext> SearchResults::contextual_results() {
  std::lock_guard<std::mutex> guard(mut_);
  std::vector<SearchResultContext> results;

  const std::string &vestibule = GetContext()->vestibule();

#if 0
  std::size_t offset_counter = 0;
#endif

  for (const auto &kv : map_) {
#if 0
    if (offset_counter < offset_) {
      offset_counter++;
      continue;
    }
#endif
    SearchResultContext context;
    context.set_filename(kv.first.filename());
    LOG(INFO) << "file_id " << kv.first.file_id() << "\n";

    // We have to get all of the lines out of the file... we're going
    // to make a map of line_num -> (is_match, line_text) and then
    // fill in the context map with that.
    std::map<std::size_t, std::pair<bool, std::string> > context_lines;
    std::ifstream ifs(vestibule + "/" + kv.first.filename(),
                      std::ifstream::in | std::ifstream::binary);
    assert(!ifs.fail());

    // For each line in the matched lines for this file...
    for (const auto &line : kv.second) {
      std::map<std::size_t, std::string> inner_context;
      try {
        inner_context = GetFileContext(&ifs, line.line_number, line.offset);
      } catch (FileError &e) {
        std::cerr << kv.first.filename() << ": " << e.what() << std::endl;
        throw;
      }
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
  return std::move(results);
}

}
