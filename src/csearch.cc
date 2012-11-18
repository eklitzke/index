// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include <map>

#include "./config.h"
#include "./context.h"
#include "./file_util.h"
#include "./ngram_index_reader.h"
#include "./util.h"

#include <string>
#include <unistd.h>

namespace po = boost::program_options;

enum class Color {
  RED   = 0,
  GREEN = 1,
  CYAN  = 2
};

const std::map<Color, std::string> color_map{
  {Color::RED, "1;31m"},
  {Color::GREEN, "0;32m"},
  {Color::CYAN, "0;36m"}};

void Colorize(std::ostream &out, Color color, const std::string &str) {
  out << "\033[" << color_map.find(color)->second << "\033[K" << str
      << "\033[m\033[K";
}

void ColorizeMatch(std::ostream &out,
                   Color color,
                   const std::string &target,
                   std::string input) {
  std::string::size_type pos = 0;
  while (true) {
    std::string::size_type offset = input.find(target, pos);
    if (offset == std::string::npos) {
      out << input.substr(pos);
      break;
    }
    out << input.substr(pos, offset - pos);
    Colorize(out, color, target);
    pos = offset + target.size();
  }
}

int main(int argc, char **argv) {
  // Declare the supported options.
  po::options_description desc("Allowed options");
  desc.add_options()
      ("help,h", "produce help message")
      ("limit", po::value<std::size_t>()->default_value(10))
      ("within-file-limit", po::value<std::size_t>()->default_value(10))
      ("offset", po::value<std::size_t>()->default_value(0))
      ("threads,t", po::value<std::size_t>()->default_value(0))
      ("no-print", "suppress printing")
      ("db-path", po::value<std::string>()->default_value(
          codesearch::default_index_directory))
      ("color", po::value<std::string>()->default_value("auto"),
       "use color -- can be yes, no, auto (default)")
      ("query,q", po::value<std::string>(),
       "(positional) the search query, mandatory")
      ;

  // all positional arguments are source dirs
  po::positional_options_description p;
  p.add("query", 1);

  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv).
            options(desc).positional(p).run(), vm);
  po::notify(vm);

  if (vm.count("help") || !vm.count("query")) {
    std::cout << desc << "\n";
    return 0;
  }

  bool colorize;
  std::string color_option = vm["color"].as<std::string>();
  if (color_option == "yes") {
    colorize = true;
  } else if (color_option == "no") {
    colorize = false;
  } else if (color_option == "auto") {
    colorize = static_cast<bool>(isatty(STDOUT_FILENO));
  } else {
    std::cout << "must use --color=yes|no|auto\n";
    return 1;
  }

  codesearch::InitializeLogging(argv[0]);

  std::string db_path_str = vm["db-path"].as<std::string>();
  std::unique_ptr<codesearch::Context> ctx(
      codesearch::Context::Acquire(db_path_str));
  codesearch::NGramIndexReader reader(
      db_path_str, vm["threads"].as<std::size_t>());

  std::size_t limit = vm["limit"].as<std::size_t>();
  codesearch::SearchResults results(
      static_cast<std::size_t>(limit),
      vm["within-file-limit"].as<std::size_t>()
      //vm["offset"].as<std::size_t>()
                                    );
  std::string query = vm["query"].as<std::string>();
  reader.Find(query, &results);
  if (!vm.count("no-print")) {
    for (const auto &sr_ctx : results.contextual_results()) {
      if (colorize) {
        Colorize(std::cout, Color::GREEN, sr_ctx.filename());
        std::cout << '\n';
      } else {
        std::cout << sr_ctx.filename() << "\n";
        std::cout << std::string(sr_ctx.filename().length(), '-') << "\n";
      }
      bool first_line = true;
      std::size_t last_line = 0;
      for (const auto &line : sr_ctx.lines()) {
        if (!first_line && line.line_num() - last_line != 1) {
          std::cout << "........\n";
        }
        if (colorize) {
          std::cout << line.line_num();
          Colorize(std::cout, Color::CYAN, ":");
          if (line.is_matched_line()) {
            ColorizeMatch(std::cout, Color::RED, query, line.line_text());
          } else {
            std::cout << line.line_text();
          }
          std::cout << "\n";
        } else {
          std::cout << (line.is_matched_line() ? "* " : "  ")
                    <<line.line_num() <<  ":" << line.line_text() << "\n";
        }
        first_line = false;
        last_line = line.line_num();
      }
      std::cout << "\n";
    }
  } else {
    std::cout << results.contextual_results().size() <<
        " search results\n";
  }
  return 0;
}
