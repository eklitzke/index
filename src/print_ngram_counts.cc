#include <boost/program_options.hpp>

#include <cstdio>
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
      ("index-directory", po::value<std::string>(),
       "(positional) the index directory")
      ;

  // positional argument is the shard
  po::positional_options_description p;
  p.add("index-directory", 1);

  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv).
            options(desc).positional(p).run(), vm);
  po::notify(vm);

  if (vm.count("help") || !vm.count("index-directory")) {
    std::cout << desc;
    return 0;
  }

  std::string ngram_counts_path = (vm["index-directory"].as<std::string>() +
                                   "/ngram_counts");
  std::ifstream ifs(ngram_counts_path,
                    std::ifstream::binary | std::ifstream::in);
  assert(!ifs.eof() && !ifs.fail());

  codesearch::NGramCounts counts;
  counts.ParseFromIstream(&ifs);
  std::cout << "distinct ngrams:   " << counts.total_ngrams() << "\n";
  std::cout << "total ngram count: " << counts.total_count() << "\n\n";

  std::uint64_t accum = 0;
  double total_count = static_cast<double>(counts.total_count());
  for (const auto &ngram : counts.ngram_counts()) {
    std::uint64_t count = ngram.count();
    accum += count;
    printf("%-10lu  %1.6f%%   %1.6f%%   %s\n", count,
           100 * static_cast<double>(count) / total_count,
           100 * static_cast<double>(accum) / total_count,
           codesearch::PrintBinaryString(ngram.ngram()).c_str());
  }
  return 0;
}
