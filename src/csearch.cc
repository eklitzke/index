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
      ("ngram-size", po::value<int>()->default_value(3), "ngram size")
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
  std::size_t ngram_size = static_cast<std::size_t>(vm["ngram-size"].as<int>());
  cs::index::IndexReader reader(db_path_str, ngram_size);
  if (!reader.Initialize()) {
    std::cerr << "Failed to initialize database reader!" << std::endl;
    return 1;
  };
  for (const auto &val : reader.Search(vm["query"].as<std::string>())) {
    std::cout << val.filename << ":" << val.line_num << ":" << val.line_text <<
        std::endl;
  }
  return 0;
}
