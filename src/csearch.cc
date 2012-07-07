// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <re2/re2.h>

#include "./index.h"

#include <string>

namespace po = boost::program_options;

int main(int argc, char **argv) {
  // Declare the supported options.
  po::options_description desc("Allowed options");
  desc.add_options()
      ("help", "produce help message")
      ("limit", po::value<std::size_t>()->default_value(0))
      ("no-print", "suppress printing")
      ("db-path", po::value<std::string>()->default_value("/tmp/index"))
      ("query,q", po::value<std::string>())
      ;

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return 0;
  }

  std::string db_path_str = vm["db-path"].as<std::string>();
  codesearch::IndexReader reader(db_path_str);
  if (!reader.Initialize()) {
    std::cerr << "Failed to initialize database reader!" << std::endl;
    return 1;
  };

  std::ios_base::sync_with_stdio(false);
  char buffer[1024];
  std::cout.rdbuf()->pubsetbuf(buffer, 1024);

  codesearch::SearchResults results(
      vm["limit"].as<std::size_t>());
  std::string query = vm["query"].as<std::string>();
  if (!reader.Search(query, &results)) {
    std::cerr << "Failed to execute query!" << std::endl;
    return 1;
  }
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
