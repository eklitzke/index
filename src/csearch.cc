// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <re2/re2.h>

#include "./context.h"
#include "./ngram_index_reader.h"

#include <string>

namespace po = boost::program_options;

int main(int argc, char **argv) {
  // Declare the supported options.
  po::options_description desc("Allowed options");
  desc.add_options()
      ("help", "produce help message")
      ("limit", po::value<std::size_t>()->default_value(100))
      ("no-print", "suppress printing")
      ("db-path", po::value<std::string>()->default_value("/tmp/index"))
      ("query,q", po::value<std::string>(), "the search query, mandatory")
      ;

  // all positional arguments are source dirs
  po::positional_options_description p;
  p.add("query", 1);

  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv).
            options(desc).positional(p).run(), vm);
  po::notify(vm);

  if (vm.count("help") || !vm.count("query")) {
    std::cout << desc << std::endl;
    return 0;
  }

  codesearch::Context ctx;

  std::string db_path_str = vm["db-path"].as<std::string>();
  codesearch::NGramIndexReader reader(db_path_str, 3);

  std::ios_base::sync_with_stdio(false);
  char buffer[1024];
  std::cout.rdbuf()->pubsetbuf(buffer, 1024);

  std::size_t limit = vm["limit"].as<std::size_t>();
  codesearch::SearchResults results(limit);
  std::string query = vm["query"].as<std::string>();
  reader.Find(query, &results);
  if (!vm.count("no-print")) {
    for (const auto &val : results.results()) {
      std::cout << val.filename() << ":" << val.line_num() <<
          ":" << val.line_text() << std::endl;
    }
  } else {
    std::cout << results.results().size() << " search results" << std::endl;
  }
  return 0;
}
