// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./rpcserver.h"

#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <boost/date_time/microsec_time_clock.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <boost/filesystem.hpp>
#include <re2/re2.h>

#include "./ngram_index_reader.h"
#include "./index.pb.h"
#include "./util.h"

#include <string>
#include <thread>

namespace codesearch {

class IndexReaderConnection {
 public:
  // Create a new IndexReaderConnection; Connections are only allowed
  // on the heap (as they must run in their own thread).
  static IndexReaderConnection *New(const std::string &db_path) {
    IndexReaderConnection *conn = new IndexReaderConnection(db_path);
    return conn;
  }

  // Start an IndexReaderConnection's io loop.
  void Start();

  boost::asio::ip::tcp::socket* socket() { return &socket_; }

  // This is public, but should only be called if Start() has not yet
  // been called.
  ~IndexReaderConnection();

 private:
  bool started_;
  boost::asio::io_service io_service_;
  std::array<char, 8> size_buffer_;
  boost::asio::streambuf data_buffer_;

  NGramIndexReader reader_;
  boost::asio::ip::tcp::socket socket_;

  IndexReaderConnection(const std::string &db_path)
      :started_(false), reader_(db_path, 3), socket_(io_service_) {}

  void Search(std::size_t size);

  void WaitForRequest();

  void SizeCallback(const boost::system::error_code& error,
                    std::size_t bytes_transferred);

  void DataCallback(const boost::system::error_code& error,
                    std::size_t bytes_transferred);

  void WriteCallback(const boost::system::error_code& error,
                     std::size_t bytes_transferred);

  void SelfDestruct();
};

void IndexReaderConnection::Start() {
  assert(started_ == false);
  started_ = true;
  WaitForRequest();
  io_service_.run();
}

void IndexReaderConnection::SizeCallback(const boost::system::error_code& error,
                                         std::size_t bytes_transferred) {
  if (error) {
    if (error == boost::asio::error::eof) {
    } else {
      std::cerr << "got error " << error << " after reading " <<
          bytes_transferred << " bytes while waiting for size header" <<
          std::endl;
    }
    SelfDestruct();
  } else {
    assert(bytes_transferred == 8);

    std::uint64_t size = ToUint64(size_buffer_);
    boost::asio::streambuf::mutable_buffers_type bufs = data_buffer_.prepare(
        size);

    boost::asio::async_read(
        socket_, bufs,
        std::bind(&IndexReaderConnection::DataCallback, this,
                  std::placeholders::_1, std::placeholders::_2));
  }
}

void IndexReaderConnection::DataCallback(const boost::system::error_code& error,
                                         std::size_t bytes_transferred) {
  if (error) {
    std::cerr << "got error " << error << "while waiting for data" <<
        std::endl;
    SelfDestruct();
  } else {
    Search(bytes_transferred);
  }
}

void IndexReaderConnection::Search(std::size_t size) {
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
    reader_.Find(search_query.query(), &results);

    SearchQueryResponse resp;
    for (const auto &result : results.results()) {
      resp.add_results()->MergeFrom(result);
    }

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

  std::cout << this << " sending response of size " << response.ByteSize() <<
      std::endl;
  boost::asio::async_write(
      socket_, data_buffer_,
      std::bind(&IndexReaderConnection::WriteCallback, this,
                std::placeholders::_1, std::placeholders::_2));
}

void IndexReaderConnection::WriteCallback(
    const boost::system::error_code& error, std::size_t bytes_transferred) {
  if (error) {
    std::cerr << "write error" << error << std::endl;
    SelfDestruct();
  } {
    data_buffer_.consume(bytes_transferred);
    io_service_.post(std::bind(&IndexReaderConnection::WaitForRequest, this));
  }
}

void IndexReaderConnection::WaitForRequest() {
  boost::asio::async_read(
      socket_, boost::asio::buffer(size_buffer_.data(), 8),
      std::bind(&IndexReaderConnection::SizeCallback, this,
                std::placeholders::_1, std::placeholders::_2));
}

void IndexReaderConnection::SelfDestruct() {
  std::cout << this << " self-destructing" << std::endl;
  started_ = false;
  delete this;
}

IndexReaderConnection::~IndexReaderConnection() {
  assert(started_ == false);
  io_service_.stop();
}


//////////////////////////////////
// INDEX READER SERVER
/////////////////////////////

void IndexReaderServer::Start() {
  StartAccept();
}

void IndexReaderServer::StartAccept() {
  IndexReaderConnection *conn = IndexReaderConnection::New(db_path_);
  acceptor_.async_accept(
      *conn->socket(),
      std::bind(&IndexReaderServer::HandleAccept, this, conn,
                std::placeholders::_1));
}

void IndexReaderServer::HandleAccept(IndexReaderConnection *conn,
                                     const boost::system::error_code& error) {
  if (error) {
    std::cerr << "unexpectedly got error " << error << std::endl;
    delete conn;
  } else {
    // Let the IndexReaderConnection execute in its own thread from
    // this point on.
    std::thread t = std::thread(&IndexReaderConnection::Start, conn);
    t.detach();
  }
  io_service_->post(std::bind(&IndexReaderServer::StartAccept, this));
}

}
