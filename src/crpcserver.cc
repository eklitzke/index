#include <boost/program_options.hpp>
#include <boost/asio.hpp>

#include "./config.h"
#include "./context.h"
#include "./rpcserver.h"
#include "./util.h"

namespace po = boost::program_options;

int main(int argc, char **argv) {
  // Declare the supported options.
  po::options_description desc("Allowed options");
  desc.add_options()
      ("help,h", "produce help message")
      ("port,p", po::value<int>()->default_value(codesearch::default_rpc_port))
      ("db-path", po::value<std::string>()->default_value(
          codesearch::default_index_directory))
      ("threads,t", po::value<std::size_t>()->default_value(0),
       "number of threads to use per index reader")
      ;

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return 0;
  }

  codesearch::InitializeLogging(argv[0]);

  std::string db_path_str = vm["db-path"].as<std::string>();
  boost::asio::io_service io_service;
  boost::asio::ip::tcp::endpoint endpoint(
      boost::asio::ip::address::from_string("127.0.0.1"), vm["port"].as<int>());
  std::size_t threads = vm["threads"].as<std::size_t>();

  std::unique_ptr<codesearch::Context> ctx(
      codesearch::Context::Acquire(db_path_str));
  ctx->InitializeSortedNGrams();
  ctx->InitializeFileOffsets();

  codesearch::IndexReaderServer server(
      db_path_str, &io_service, endpoint, threads);
  server.Start();
  io_service.run();

  return 0;
}
