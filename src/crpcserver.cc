#include <boost/program_options.hpp>
#include <boost/asio.hpp>
#include <glog/logging.h>

#include "./config.h"
#include "./context.h"
#include "./rpcserver.h"

namespace po = boost::program_options;

int main(int argc, char **argv) {
  // Declare the supported options.
  po::options_description desc("Allowed options");
  desc.add_options()
      ("help,h", "produce help message")
      ("port,p", po::value<int>()->default_value(9900))
      ("db-path", po::value<std::string>()->default_value(
          codesearch::default_index_directory))
      ;

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return 0;
  }

  google::InitGoogleLogging(argv[0]);

  std::string db_path_str = vm["db-path"].as<std::string>();
  std::unique_ptr<codesearch::Context> ctx(
      codesearch::Context::Acquire(db_path_str));

  boost::asio::io_service io_service;
  boost::asio::ip::tcp::endpoint endpoint(
      boost::asio::ip::address::from_string("127.0.0.1"), vm["port"].as<int>());

  codesearch::IndexReaderServer server(db_path_str, &io_service, endpoint);
  server.Start();
  io_service.run();

  return 0;
}
