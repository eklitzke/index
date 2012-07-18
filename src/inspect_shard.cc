#include <boost/program_options.hpp>

#include <array>
#include <iostream>
#include <fstream>

#include <ctype.h>

#include "./index.pb.h"
#include "./util.h"

namespace po = boost::program_options;

void PrintBinaryString(const std::string &str) {
  for (const auto &c : str) {
    if (isgraph(c)) {
      putchar(c);
    } else {
      printf("\\x%02x", static_cast<unsigned int>(c));
    }
  }
}

int main(int argc, char **argv) {
  // Declare the supported options.
  po::options_description desc("Allowed options");
  desc.add_options()
      ("help,h", "produce help message")
      ("shard,s", po::value<std::string>(), "(positional) the shard to inspect")
      ("integer,i", "interpret values as integers")
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

  std::array<char, sizeof(std::uint64_t)> hdr_size;
  std::ifstream ifs(shard, std::ifstream::binary | std::ifstream::in);
  ifs.read(hdr_size.data(), sizeof(std::uint64_t));
  assert(!ifs.eof() && !ifs.fail());
  std::uint64_t hdr_size_val = codesearch::ToUint64(hdr_size);
  if (hdr_size_val > 8096) {
    std::cerr << shard << " had implausible hdr_size_val " << hdr_size_val <<
        ", cowardly aborting" << std::endl;
    return 1;
  }

  std::unique_ptr<char []> hdr_data(new char[hdr_size_val]);
  ifs.read(hdr_data.get(), hdr_size_val);
  assert(!ifs.eof() && !ifs.fail());

  std::string hdr_string(hdr_data.get(), hdr_size_val);
  codesearch::SSTableHeader hdr;
  hdr.ParseFromString(hdr_string);
  assert(hdr.min_value().size() == hdr.key_size());
  assert(hdr.max_value().size() == hdr.key_size());
  assert(memcmp(
      hdr.min_value().data(), hdr.max_value().data(), hdr.key_size()) <= 0);

  std::cout << "key_size   = " << hdr.key_size() << std::endl;
  std::cout << "num_keys   = " << hdr.num_keys() << std::endl;
  std::cout << "index_size = " << hdr.index_size() << std::endl;
  std::cout << "data_size  = " << hdr.data_size() << std::endl;
  if (vm.count("integer")) {
    std::array<std::uint8_t, sizeof(std::uint64_t)> min_value, max_value;
    memcpy(min_value.data(), hdr.min_value().data(), 8);
    memcpy(max_value.data(), hdr.max_value().data(), 8);
    std::cout << "min_value  = " << codesearch::ToUint64(min_value) << "\n";
    std::cout << "max_value  = " << codesearch::ToUint64(max_value) << "\n";
  } else {
    std::cout << "min_value = ";
    PrintBinaryString(hdr.min_value());
    std::cout << std::endl;
    std::cout << "max_value = ";
    PrintBinaryString(hdr.max_value());
    std::cout << std::endl;
  }
  return 0;
}
