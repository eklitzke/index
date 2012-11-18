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
      ("config,c", po::value<std::string>(),
       "(positional) the config file to inspect")
      ;

  // positional argument is the shard
  po::positional_options_description p;
  p.add("config", 1);

  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv).
            options(desc).positional(p).run(), vm);
  po::notify(vm);

  if (vm.count("help") || !vm.count("config")) {
    std::cout << desc;
    return 0;
  }

  std::string config_path = vm["config"].as<std::string>();
  codesearch::IndexConfig config;
  {
    std::ifstream ifs(config_path, std::ifstream::in | std::ifstream::binary);
    config.ParseFromIstream(&ifs);
  }
  std::cout << "shard_size: " << config.shard_size() << "\n";
  std::cout << "num_shards: " << config.num_shards() << "\n";
  std::cout << "state:      ";
  switch (config.state()) {
    case codesearch::IndexConfig::EMPTY:
      std::cout << "EMPTY\n";
      break;
    case codesearch::IndexConfig::BUILDING:
      std::cout << "BUILDING\n";
      break;
    case codesearch::IndexConfig::COMPLETE:
      std::cout << "COMPLETE\n";
      break;
  }
  std::cout << "key_type:   ";
  switch (config.key_type()) {
    case codesearch::IndexConfig::NUMERIC:
      std::cout << "NUMERIC\n";
      break;
    case codesearch::IndexConfig::STRING:
      std::cout << "STRING\n";
      break;
  }
  return 0;
}
