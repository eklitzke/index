// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>

#include <iostream>
#include <fstream>
#include <string>

#include "./config.h"
#include "./index.pb.h"
#include "./util.h"

namespace po = boost::program_options;

int main(int argc, char **argv) {
  // Declare the supported options.
  po::options_description desc("Allowed options");
  desc.add_options()
      ("help,h", "produce help message")
      ("port,p", po::value<int>()->default_value(codesearch::default_rpc_port),
       "the port to connect on")
      ("limit", po::value<std::size_t>()->default_value(20),
       "the number of responses to return")
      ("show-sizes", "show response sizes")
      ("file,f", po::value<std::string>(), "the query file  (default is stdin)")
      ;

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return 0;
  }

  bool delete_input = false;
  std::istream *input;
  if (vm.count("file")) {
    input = new std::ifstream(vm["file"].as<std::string>());
    delete_input = true;
  } else {
    input = &std::cin;
  }

  std::cout << "connecting... " << std::flush;
  codesearch::Timer connect_timer;

  boost::asio::io_service io;

  boost::asio::ip::tcp::resolver resolver(io);
  boost::asio::ip::tcp::resolver::query query(
      "127.0.0.1", boost::lexical_cast<std::string>(vm["port"].as<int>()));

  boost::asio::ip::tcp::socket socket(io);
  boost::asio::connect(socket, resolver.resolve(query));
  std::cout << "connected in " << connect_timer.elapsed_us() << " us" <<
      std::endl;

  const bool show_sizes = vm.count("show-sizes") > 0;
  const std::size_t limit = vm["limit"].as<std::size_t>();
  std::size_t search_count = 0;
  codesearch::Timer total_timer;

  while (true) {
    std::string line;
    std::getline(*input, line);
    if (input->fail() || input->eof()) {
      break;
    }
    if (line.empty() || line[0] == '#') {
      continue;
    }
    for (std::string::size_type i = 1; i <= line.size(); i++) {
      search_count++;
      std::string query = line.substr(0, i);

      codesearch::RPCRequest request;
      codesearch::SearchQueryRequest *search_request;
      search_request = request.mutable_search_query();
      search_request->set_query(query);
      search_request->set_limit(limit);

      std::string serialized_req;
      request.SerializeToString(&serialized_req);

      std::string request_buf = codesearch::Uint64ToString(
          serialized_req.size());
      request_buf += serialized_req;

      std::cout << query << " " << std::flush;

      codesearch::Timer timer;
      boost::system::error_code error;
      boost::asio::write(socket, boost::asio::buffer(request_buf),
                         boost::asio::transfer_all(), error);
      assert(!error);

      static char response_size[sizeof(std::uint64_t)];
      std::size_t bytes_read = socket.read_some(
          boost::asio::buffer(response_size, sizeof(response_size)), error);
      assert(!error);
      assert(bytes_read == sizeof(response_size));

      std::uint64_t response_size_int = codesearch::ToUint64(
          std::string(response_size, 8));

      std::unique_ptr<char[]> full_response(new char[response_size_int]);
      boost::asio::read(
          socket, boost::asio::buffer(full_response.get(), response_size_int));

      if (show_sizes) {
        std::cout << response_size_int << " ";
      }
      std::cout << timer.elapsed_ms() << std::endl;
    }
  }

  long total_ms = total_timer.elapsed_ms();
  std::size_t avg_ms = total_ms / search_count;

  double secs = static_cast<double>(total_ms) / 1000.0;
  double qps = search_count / secs;

  std::cout << "\n" << search_count << " total searches in " <<
      total_timer.elapsed_ms() << " ms (avg " << avg_ms << " ms per query / " <<
      qps << " qps)" << std::endl;
  if (delete_input) {
    delete input;
  }
  return 0;
}
