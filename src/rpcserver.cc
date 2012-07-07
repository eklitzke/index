// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <boost/date_time/microsec_time_clock.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <boost/filesystem.hpp>
#include <re2/re2.h>

#include "./index.h"
#include "./index.pb.h"

#include <string>

namespace codesearch {

class IndexReaderConnection {
 public:
  IndexReaderConnection(const std::string &db_path,
                        boost::asio::io_service *io_service)
      :reader_(db_path), socket_(*io_service) {
    assert(reader_.Initialize());
  }

  void SizeCallback(const boost::system::error_code& error,
                    std::size_t bytes_transferred) {
    if (error) {
      if (error == boost::asio::error::eof) {
        std::cout << "size header, connection reset by peer" << std::endl;
      } else {
        std::cout << "got error " << error << " after reading " <<
            bytes_transferred << " bytes while waiting for size header" <<
            std::endl;
      }
      SelfDestruct();
    } else {
      assert(bytes_transferred == 8);

      std::uint64_t size = ToUint64(size_buffer_);
      std::cout << this << " got a query of size " << size << ", reading" <<
          std::endl;

      boost::asio::streambuf::mutable_buffers_type bufs = data_buffer_.prepare(
          size);

      boost::asio::async_read(
          socket_, bufs,
          std::bind(&IndexReaderConnection::DataCallback, this,
                    std::placeholders::_1, std::placeholders::_2));
    }
  }

  void DataCallback(const boost::system::error_code& error,
                    std::size_t bytes_transferred) {
    std::cout << "in data callback, bytes_transferred = " <<
        bytes_transferred << std::endl;
    if (error) {
      std::cout << "got error " << error << "while waiting for data" <<
          std::endl;
      SelfDestruct();
    } else {
      Search(bytes_transferred);
    }
  }

  void Search(std::size_t size) {
    data_buffer_.commit(size);

    RPCRequest request;
    std::istream is(&data_buffer_);
    request.ParseFromIstream(&is);

    RPCResponse response;
    response.set_request_num(request.request_num());

    if (request.has_search_query()) {
      boost::posix_time::ptime start_time(
          boost::posix_time::microsec_clock::universal_time());

      const SearchQueryRequest &search_query = request.search_query();
      std::cout << this << " doing search query, request_num = " <<
          request.request_num() << ", query = \"" <<
          search_query.query() << "\"" << std::endl;

      SearchResults results(search_query.limit());
      if (!reader_.Search(search_query.query(), &results)) {
        std::cerr << "Failed to execute query!" << std::endl;
        SelfDestruct();
        return;
      }

      SearchQueryResponse resp;
      for (const auto &result : results.results()) {
        resp.add_results()->MergeFrom(result);
      }
      resp.set_total_results(results.attempted());

      boost::posix_time::ptime end_time(
          boost::posix_time::microsec_clock::universal_time());
      boost::posix_time::time_duration duration = end_time - start_time;
      resp.set_time_elapsed(duration.total_milliseconds());

      response.mutable_search_response()->MergeFrom(resp);

    } else {
      std::cerr << this << " don't know how to handle queries of that type" <<
          std::endl;
      SelfDestruct();
      return;
    }

    std::string size_header;
    Uint64ToString(response.ByteSize(), &size_header);

    std::ostream os(&data_buffer_);
    os << size_header;
    response.SerializeToOstream(&os);

    boost::asio::async_write(
        socket_, data_buffer_,
        std::bind(&IndexReaderConnection::WriteCallback, this,
                  std::placeholders::_1, std::placeholders::_2));
  }

  void WriteCallback(const boost::system::error_code& error,
                     std::size_t bytes_transferred) {
    if (error) {
      std::cerr << "write error" << error << std::endl;
      SelfDestruct();
    } {
      data_buffer_.consume(bytes_transferred);
      Start();
    }
  }

  void Start() {
    std::cout << this << " waiting for request..." << std::endl;
    boost::asio::async_read(
        socket_, boost::asio::buffer(size_buffer_.data(), 8),
        std::bind(&IndexReaderConnection::SizeCallback, this,
                  std::placeholders::_1, std::placeholders::_2));
  }

  void SelfDestruct() {
    std::cout << "self destructing!" << std::endl;
    delete this;
  }

  boost::asio::ip::tcp::socket* socket() { return &socket_; }
 private:
  std::array<char, 8> size_buffer_;
  boost::asio::streambuf data_buffer_;

  IndexReader reader_;
  boost::asio::ip::tcp::socket socket_;
};

class IndexReaderServer {
 public:
  IndexReaderServer(const std::string &db_path,
                    boost::asio::io_service* io_service,
                    const boost::asio::ip::tcp::endpoint &endpoint)
      :db_path_(db_path), io_service_(io_service),
       acceptor_(*io_service, endpoint) {}

  void Start() {
    StartAccept();
  }

  void StartAccept() {
    IndexReaderConnection *conn = new IndexReaderConnection(
        db_path_, io_service_);
    acceptor_.async_accept(
        *conn->socket(),
        std::bind(&IndexReaderServer::HandleAccept, this, conn,
                  std::placeholders::_1));
  }

  void HandleAccept(IndexReaderConnection *conn,
                    const boost::system::error_code& error) {
    if (error) {
      std::cerr << "unexpectedly got error " << error << std::endl;
      delete conn;
    } else {
      conn->Start();
    }
    StartAccept();
  }

 private:
  std::string db_path_;
  boost::asio::io_service *io_service_;
  boost::asio::ip::tcp::acceptor acceptor_;
};

}

namespace po = boost::program_options;

int main(int argc, char **argv) {
  // Declare the supported options.
  po::options_description desc("Allowed options");
  desc.add_options()
      ("help", "produce help message")
      ("port,p", po::value<int>()->default_value(9001))
      ("db-path", po::value<std::string>()->default_value("/tmp/index"))
      ;

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return 0;
  }

  std::string db_path_str = vm["db-path"].as<std::string>();

  boost::asio::io_service io_service;
  boost::asio::ip::tcp::endpoint endpoint(
      boost::asio::ip::address::from_string("127.0.0.1"), vm["port"].as<int>());
  codesearch::IndexReaderServer server(db_path_str, &io_service, endpoint);
  server.Start();
  io_service.run();

  return 0;
}
