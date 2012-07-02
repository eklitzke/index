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
      ("replace", "replace the directory contents")
      ("ngram-size", po::value<int>()->default_value(3), "ngram size")
      ("db-path", po::value<std::string>()->default_value("/tmp/index"))
      ("src-dir,s",
       po::value<std::vector<std::string > >(), "source directories")
      ;

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
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
  std::unique_ptr<codesearch::IndexWriter> writer(
      new codesearch::IndexWriter(db_path_str, ngram_size));
  if (!writer->Initialize()) {
    std::cerr << "Failed to initialize database writer! Check that \"" <<
        vm["db-path"].as<std::string>() << "\" exists and is empty." <<
        std::endl;
    return 1;
  };

  re2::RE2 interesting_file_re(".*(\\.h|\\.cc|\\.c|\\.clj)$");
  if (!interesting_file_re.ok()) {
    std::cerr << "Failed to initialize re2!" << std::endl;
    return 1;
  }

  for (const auto &d : vm["src-dir"].as<std::vector<std::string >>()) {
    std::string match;
    for (boost::filesystem::recursive_directory_iterator end, it(d);
         it != end; ++it) {
      const std::string &filepath = it->path().string();
      if (RE2::FullMatch(filepath, interesting_file_re, &match)) {
        const std::string canonical = boost::filesystem::canonical(
            it->path()).string();
        std::cout << "indexing " << canonical << std::endl;
        writer->AddFile(canonical);
      }
    }
  }
  std::cout << "finishing database..." << std::endl;
  delete writer.release();
  std::cout << "done indexing" << std::endl;
  return 0;
}
