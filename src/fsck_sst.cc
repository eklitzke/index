#include <boost/program_options.hpp>

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
      ("verbose,v", "run verbosely")
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

  const bool verbose = vm.count("verbose");
  std::string shard = vm["shard"].as<std::string>();
  std::ifstream ifs(shard, std::ifstream::in | std::ifstream::binary);
  codesearch::SSTableHeader hdr = codesearch::ReadHeader(&ifs);
  std::uint64_t file_size = codesearch::FileSize(&ifs);

  ifs.seekg(hdr.index_offset());
  assert(hdr.index_size() ==
         (hdr.key_size() + sizeof(std::uint64_t)) * hdr.num_keys());
  const std::size_t key_size = hdr.key_size();

  std::unique_ptr<char[]> key_data(new char[key_size]);
  std::string key;
  std::string last_key;
  codesearch::ExpandableBuffer valdata;
  for (std::size_t i = 0; i < hdr.num_keys(); i++) {
    std::streampos keyoffset = ifs.tellg();
    ifs.read(key_data.get(), key_size);
    assert(!(ifs.fail() || ifs.eof()));
    key.clear();
    key.insert(0, key_data.get(), key_size);
    if (verbose) {
      std::cout << "key: " << codesearch::PrintBinaryString(key) <<
          " (num: " << i << ", key offset: " <<
          (static_cast<std::size_t>(keyoffset) - hdr.index_offset()) <<
          ", absolute offset: " << keyoffset << ")\n";
    }

    assert(last_key <= key);
    last_key = key;

    std::uint64_t data_offset = codesearch::ReadUint64(&ifs);
    assert(!(ifs.fail() || ifs.eof()));
    assert(data_offset < UINT32_MAX);  // sanity check

    std::streampos p = ifs.tellg();
    assert(p > 0);
    assert(hdr.data_offset() + data_offset < file_size);
    ifs.seekg(hdr.data_offset() + data_offset);
    assert(!ifs.fail());

    std::uint32_t val_size = codesearch::ReadUint32(&ifs);
    assert(val_size < (16 << 20));  // sanity check
    std::cout << "val: " << val_size << "bytes (offset: " <<
        (hdr.data_offset() + data_offset) << ")\n" << std::endl;
    ifs.seekg(p);
  }
  return 0;
}
