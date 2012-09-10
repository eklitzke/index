// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include "./config.h"
#include "./context.h"
#include "./file_util.h"
#include "./ngram_index_writer.h"
#include "./ngram_counter.h"
#include "./index.pb.h"
#include "./util.h"

#include <string>
#include <thread>
#include <fstream>

namespace po = boost::program_options;

namespace {
struct FileTuple {
  FileTuple(const std::string &c,
            const std::string &d,
            const std::string &f)
      :canonical(c), dir(d), fname(f) {}

  std::string canonical;
  std::string dir;
  std::string fname;

  bool operator<(const FileTuple &other) const {
    return std::make_tuple(canonical, dir, fname) <
      std::make_tuple(other.canonical, other.dir, other.fname);
  }
};
}

int main(int argc, char **argv) {
  // Declare the supported options.
  po::options_description desc("Allowed options");
  desc.add_options()
      ("help,h", "produce help message")
      ("replace,r", "replace the directory contents")
      ("ngram-size", po::value<std::size_t>()->default_value(3), "ngram size")
      ("db-path", po::value<std::string>()->default_value(
          codesearch::default_index_directory))
      ("shard-size", po::value<std::size_t>()->default_value(16<<20))
      ("src-dir,s",
       po::value<std::vector<std::string > >(),
       "(positional) source directories")
      ("threads,t", po::value<std::size_t>()->default_value(
          std::thread::hardware_concurrency()))
      ;

  // all positional arguments are source dirs
  po::positional_options_description p;
  p.add("src-dir", -1);

  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv).
            options(desc).positional(p).run(), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return 0;
  }

  codesearch::InitializeLogging(argv[0]);

  std::string db_path_str = vm["db-path"].as<std::string>();
  boost::filesystem::path db_path(db_path_str);
  if (boost::filesystem::exists(db_path)) {
    if (vm.count("replace")) {
      // Delete all of the subdirectories and files contained within
      // the index directory.
      for (boost::filesystem::directory_iterator end, it(db_path);
           it != end; ++it) {
        boost::filesystem::remove_all(it->path());
      }
    } else {
      std::cerr << "database at " << db_path << " already exists, use "
          "-r/--replace if you wish to replace it" << std::endl;
      return 1;
    }
  } else {
    // The index directory needs to be created
    boost::filesystem::create_directories(db_path);
  }

  std::size_t ngram_size = vm["ngram-size"].as<std::size_t>();
  std::size_t shard_size = vm["shard-size"].as<std::size_t>();
  std::size_t num_threads = vm["threads"].as<std::size_t>();

  if (vm.count("src-dir") != 1) {
    std::cerr << "Must specify exactly one src-dir/vestibule" << std::endl;
    return 1;
  }
  const std::vector<std::string> src_dirs = vm["src-dir"].as<
    std::vector<std::string > >();
  std::vector<FileTuple> to_index;
  std::unique_ptr<codesearch::Context> ctx(
      codesearch::Context::Acquire(db_path_str, ngram_size, src_dirs[0], true));
  std::cout << "collecting paths of files to index..." << std::endl;
  for (const auto &d : src_dirs) {
    std::string dir = d.substr(0, d.find_last_not_of('/') + 1);
    std::string match;
    for (boost::filesystem::recursive_directory_iterator end, it(dir);
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
      if (!codesearch::IsValidUtf8(canonical)) {
        // Normally filenames on a modern linux system are utf-8 by
        // convention, but there's nothing that requires this (they
        // can essentially be any sequence of non-null
        // characters). Skip files whose names are not a valid utf-8
        // sequence.
        std::cout << it->path() << " is not a valid UTF-8 sequence" <<
            std::endl;
        continue;
      }
      std::string fname = filepath.substr(dir.size(), std::string::npos);
      fname = fname.substr(fname.find_first_not_of('/'), std::string::npos);
      if (codesearch::ShouldIndex(filepath)) {
        to_index.push_back(FileTuple(canonical, dir, fname));
      } else {
        std::cout << "skipping " << fname << "\n";
      }
    }
  }
  std::cout << "sorting " << to_index.size() << "files..." << std::endl;
  std::sort(to_index.begin(), to_index.end());
  {
    std::size_t filenum = 1;
    codesearch::NGramIndexWriter ngram_writer(
        db_path_str, ngram_size, shard_size, num_threads);
    for (const FileTuple &tuple : to_index) {
      int pct = 100 * filenum / to_index.size();
      std::cout << "indexing " << filenum++ << "/" << to_index.size() <<
          " (" << pct << "%) " << tuple.fname << std::endl;
      ngram_writer.AddFile(tuple.canonical, tuple.dir, tuple.fname);
    }
    std::cout << "finishing database..." << std::endl;
  }
  std::cout << "writing counts..." << std::endl;
  std::ofstream ngram_counts(db_path_str + "/ngram_counts",
                std::ofstream::binary | std::ofstream::trunc |
                std::ofstream::out);
  codesearch::NGramCounter *counter = codesearch::NGramCounter::Instance();
  codesearch::NGramCounts counts = counter->ReverseSortedCounts();
  counts.set_total_count(counter->TotalCount());
  counts.set_total_ngrams(counter->TotalNGrams());
  counts.SerializeToOstream(&ngram_counts);
  std::cout << "done indexing" << std::endl;
  return 0;
}
