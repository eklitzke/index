// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include <boost/program_options.hpp>

#include <fstream>
#include <iostream>
#include <string>

#include "./index.pb.h"

namespace po = boost::program_options;

int main(int argc, char **argv) {
  // Declare the supported options.
  po::options_description desc("Allowed options");
  desc.add_options()
      ("help,h", "produce help message")
      ("metaconfig,c", po::value<std::string>(),
       "(positional) the config file to inspect")
      ;

  // positional argument is the shard
  po::positional_options_description p;
  p.add("metaconfig", 1);

  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv).
            options(desc).positional(p).run(), vm);
  po::notify(vm);

  if (vm.count("help") || !vm.count("metaconfig")) {
    std::cout << desc;
    return 0;
  }

  std::string config_path = vm["metaconfig"].as<std::string>();
  codesearch::MetaIndexConfig config;
  {
    std::ifstream ifs(config_path, std::ifstream::in | std::ifstream::binary);
    config.ParseFromIstream(&ifs);
  }
  std::cout << "vestibule:  " << config.vestibule() << "\n";
  std::cout << "ngram_size: " << config.ngram_size() << "\n";
  return 0;
}
