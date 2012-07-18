// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include "./file_types.h"
#include "./ngram_index_writer.h"
#include "./ngram_counter.h"
#include "./index.pb.h"

#include <string>
#include <fstream>

namespace po = boost::program_options;

int main(int argc, char **argv) {
  // Declare the supported options.
  po::options_description desc("Allowed options");
  desc.add_options()
      ("help", "produce help message")
      ("replace,r", "replace the directory contents")
      ("ngram-size", po::value<int>()->default_value(3), "ngram size")
      ("db-path", po::value<std::string>()->default_value("/tmp/index"))
      ("shard-size", po::value<std::size_t>()->default_value(16<<20))
      ("src-dir,s",
       po::value<std::vector<std::string > >(), "source directories")
      ;

  // all positional arguments are source dirs
  po::positional_options_description p;
  p.add("src-dir", -1);

  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv).
            options(desc).positional(p).run(), vm);
  po::notify(vm);

  std::ios_base::sync_with_stdio(false);

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return 0;
  }

  std::string db_path_str = vm["db-path"].as<std::string>();
  boost::filesystem::path db_path(db_path_str);
  if (vm.count("replace")) {
    boost::filesystem::remove_all(db_path);
  }
  boost::filesystem::create_directories(db_path);

  std::size_t ngram_size = static_cast<std::size_t>(vm["ngram-size"].as<int>());

  std::size_t shard_size = vm["shard-size"].as<std::size_t>();
  {
    codesearch::NGramIndexWriter ngram_writer(
        db_path_str, ngram_size, shard_size);

    for (const auto &d : vm["src-dir"].as<std::vector<std::string >>()) {
      std::string match;
      for (boost::filesystem::recursive_directory_iterator end, it(d);
           it != end; ++it) {
        if (!boost::filesystem::is_regular_file(it->path())) {
          continue;
        }
        const std::string &filepath = it->path().string();
        boost::system::error_code ec;
        const std::string canonical = boost::filesystem::canonical(
            it->path(), ec).string();
        if (ec) {
          continue;
        }
        if (codesearch::ShouldIndex(filepath)) {
          std::cout << "indexing " << canonical << std::endl;
          ngram_writer.AddFile(canonical);
        } else {
          std::cout << "skipping " << canonical << std::endl;
        }
      }
    }
    std::cout << "finishing database..." << std::endl;
  }
  std::cout << "writing counts..." << std::endl;
  std::ofstream ngram_counts(db_path_str + "/ngram_counts",
                std::ofstream::binary | std::ofstream::trunc |
                std::ofstream::out);
  codesearch::NGramCounter *counter = codesearch::NGramCounter::Instance();
  codesearch::NGramCounts counts = counter->ReverseSortedCounts();
  counts.SerializeToOstream(&ngram_counts);
  std::cout << "done indexing" << std::endl;
  return 0;
}
