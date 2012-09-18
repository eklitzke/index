#include <boost/program_options.hpp>

#include <array>
#include <iostream>
#include <fstream>

#include "./index.pb.h"
#include "./util.h"

namespace po = boost::program_options;

int main(int argc, char **argv) {
  // Declare the supported options.
  po::options_description desc("Allowed options");
  desc.add_options()
      ("help,h", "produce help message")
      ("shard,s", po::value<std::string>(), "(positional) the shard to inspect")
      ;

  // positional argument is the shard
  po::positional_options_description p;
  p.add("shard", 1);

  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv).
            options(desc).positional(p).run(), vm);
  po::notify(vm);

  if (vm.count("help") || !vm.count("shard")) {
    std::cout << desc;
    return 0;
  }

  std::string shard = vm["shard"].as<std::string>();
  codesearch::SSTableHeader hdr;
  std::size_t file_size;
  {
    std::ifstream ifs(shard, std::ifstream::in | std::ifstream::binary);
    hdr = codesearch::ReadHeader(&ifs);
    file_size = codesearch::FileSize(&ifs);
  }
  assert(hdr.min_value().size() == hdr.key_size());
  assert(hdr.max_value().size() == hdr.key_size());
  assert(memcmp(
      hdr.min_value().data(), hdr.max_value().data(), hdr.key_size()) <= 0);
  assert(hdr.index_offset() + hdr.index_size() + hdr.data_size() == file_size);
  assert(hdr.data_offset() + hdr.data_size() == file_size);

  std::cout << "key_size     = " << hdr.key_size() << "\n";
  std::cout << "num_keys     = " << hdr.num_keys() << "\n";
  std::cout << "index_size   = " << hdr.index_size() << "\n";
  std::cout << "data_size    = " << hdr.data_size() << "\n";
  std::cout << "index_offset = " << hdr.index_offset() << "\n";
  std::cout << "data_offset  = " << hdr.data_offset() << "\n";

  // Now try to print the min/max value. To do this correctly we need
  // to read the config file in the containing directory, to get a
  // hint for the key type.
  std::string dirname;
  std::string::size_type slashpos = shard.find_last_of('/');
  if (slashpos == std::string::npos) {
    dirname = "./";
  } else {
    dirname = shard.substr(0, slashpos + 1);
  }
  assert(dirname.back() == '/');
  std::ifstream config_ifs(dirname + "config",
                           std::ifstream::binary | std::istream::in);
  codesearch::IndexConfig config;
  config.ParseFromIstream(&config_ifs);

  if (config.key_type() == codesearch::IndexConfig_KeyType_NUMERIC) {
    std::array<std::uint8_t, sizeof(std::uint64_t)> min_value, max_value;
    memcpy(min_value.data(), hdr.min_value().data(), 8);
    memcpy(max_value.data(), hdr.max_value().data(), 8);
    std::cout << "min_value    = " << codesearch::ToUint64(min_value) << "\n";
    std::cout << "max_value    = " << codesearch::ToUint64(max_value) << "\n";
  } else {
    std::cout << "min_value    = ";
    std::cout << codesearch::PrintBinaryString(hdr.min_value()) << std::endl;
    std::cout << "max_value    = ";
    std::cout << codesearch::PrintBinaryString(hdr.max_value()) << std::endl;
  }
  return 0;
}
