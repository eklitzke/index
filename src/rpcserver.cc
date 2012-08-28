// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./rpcserver.h"

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <glog/logging.h>
#ifdef USE_SNAPPY
#include <snappy.h>
#endif  // USE_SNAPPY

#include "./ngram_index_reader.h"
#include "./index.pb.h"
#include "./util.h"

#include <iomanip>
#include <string>
#include <thread>

namespace {
std::set<codesearch::IndexReaderConnection *> conns;
std::mutex conns_mut;
}

namespace codesearch {

class IndexReaderConnection {
 public:
  IndexReaderConnection(const std::string &db_path)
      :started_(false), reader_(db_path), socket_(io_service_) {}

  // Start an IndexReaderConnection's io loop.
  void Start();

  void Stop() {
    SelfDestruct();
  }

  boost::asio::ip::tcp::socket* socket() { return &socket_; }

  // This is public, but should only be called if Start() has not yet
  // been called.
  ~IndexReaderConnection();

 private:
  bool started_;
  boost::asio::io_service io_service_;
  std::array<char, sizeof(std::uint64_t)> size_buffer_;
  boost::asio::streambuf data_buffer_;

  NGramIndexReader reader_;
  boost::asio::ip::tcp::socket socket_;

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

void IndexReaderConnection::WaitForRequest() {
  boost::asio::async_read(socket_,
                          boost::asio::buffer(size_buffer_.data(),
                                              sizeof(std::uint64_t)),
                          std::bind(&IndexReaderConnection::SizeCallback, this,
                                    std::placeholders::_1,
                                    std::placeholders::_2));
}

void IndexReaderConnection::SizeCallback(const boost::system::error_code& error,
                                         std::size_t bytes_transferred) {
  if (error) {
    if (error == boost::asio::error::eof) {
    } else {
      LOG(ERROR) << "got error " << error << " after reading " <<
          bytes_transferred << " bytes while waiting for size header\n";
    }
    SelfDestruct();
  } else {
    assert(bytes_transferred == sizeof(std::uint64_t));

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
    LOG(ERROR) << "got error " << error << "while waiting for data\n";
    SelfDestruct();
  } else {
    Search(bytes_transferred);
  }
}

void IndexReaderConnection::Search(std::size_t size) {
  Timer timer;

  data_buffer_.commit(size);

  RPCRequest request;
  std::istream is(&data_buffer_);
  request.ParseFromIstream(&is);

  RPCResponse response;
  response.set_request_num(request.request_num());
  SearchQueryResponse *resp;

  if (request.has_search_query()) {
    const SearchQueryRequest &search_query = request.search_query();
    LOG(INFO) << this << " doing search query, request_num = " <<
        request.request_num() << ", query = \"" <<
        search_query.query() << "\", offset = " <<
        search_query.offset() << ", limit = " <<
        search_query.limit() << "\n";

    SearchResults results(search_query.limit(),
                          search_query.within_file_limit()
                          //search_query.offset()
                          );
    reader_.Find(search_query.query(), &results);

    resp = response.mutable_search_response();
    for (const auto &result : results.contextual_results()) {
      resp->add_results()->MergeFrom(result);
    }
  } else {
    LOG(WARNING) << this << " don't know how to handle queries of that type\n";
    SelfDestruct();
    return;
  }
  response.set_time_elapsed(timer.elapsed_ms());

#ifdef USE_SNAPPY
  std::string uncompressed_response, compressed_response;
  response.SerializeToString(&uncompressed_response);
  snappy::Compress(uncompressed_response.data(), uncompressed_response.size(),
                   &compressed_response);
  double speedup = static_cast<double>(uncompressed_response.size()) /
      static_cast<double>(compressed_response.size());

  std::string size_header = Uint64ToString(compressed_response.size());
  std::ostream os(&data_buffer_);
  os << size_header << compressed_response;

  LOG(INFO) << this << " sending response of size " <<
      compressed_response.size() << " (speedup " <<
      std::setiosflags(std::ios::fixed) << std::setprecision(2) << speedup <<
      "x)" << " after " << response.time_elapsed() << " ms\n";

#else
   std::string size_header = Uint64ToString(response.ByteSize());
   assert(size_header.size() == sizeof(std::uint64_t));

   std::ostream os(&data_buffer_);
   os << size_header;
   response.SerializeToOstream(&os);
   LOG(INFO) << this << " sending response of size " << response.ByteSize() <<
       " after " << resp->time_elapsed() << " ms\n";

#endif

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

void IndexReaderConnection::SelfDestruct() {
  LOG(INFO) << this << " self-destructing\n";
  started_ = false;
  io_service_.stop();
}

IndexReaderConnection::~IndexReaderConnection() {
  std::lock_guard<std::mutex> guard(conns_mut);
  conns.erase(this);
  assert(started_ == false);
}
}

namespace {
codesearch::IndexReaderConnection* NewConnection(const std::string &s) {
  codesearch::IndexReaderConnection *conn;
  conn = new codesearch::IndexReaderConnection(s);
  std::lock_guard<std::mutex> guard(conns_mut);
  conns.insert(conn);
  return conn;
}

void LaunchConnection(codesearch::IndexReaderConnection *conn) {
  conn->Start();
  delete conn;
  LOG(INFO) << "thread ending\n";
}

// Force all connections to stop. The main reason that we would want
// to do this is to assist in valgrinding, if we have extra threads
// left over when the process exits then valgrind thinks that the
// memory allocated by those threads leaked.
void DeleteAllConnections() {
  std::set<codesearch::IndexReaderConnection*> conns_copy;
  {
    std::lock_guard<std::mutex> guards(conns_mut);
    conns_copy = conns;
  }
  for (const auto &c : conns_copy) {
    c->Stop();
  }
  //std::lock_guard<std::mutex> guards(conns_mut);
  //assert(conns.size() == 0);
}
}

//////////////////////////////////
// INDEX READER SERVER
/////////////////////////////

namespace codesearch {

void IndexReaderServer::Start() {
  LOG(INFO) << "now accepting RPC connections\n";
  StartAccept();
}

void IndexReaderServer::StartAccept() {
  conn_ = NewConnection(db_path_);
  acceptor_.async_accept(
      *conn_->socket(),
      std::bind(&IndexReaderServer::HandleAccept, this,
                std::placeholders::_1));
}

void IndexReaderServer::HandleAccept(const boost::system::error_code& error) {
  conn_count_++;
#if 0
  // This is a hacky way of debugging with massif
  if (conn_count_ == 2) {
    return;
  }
#endif
  if (error) {
    LOG(ERROR) << "unexpectedly got error " << error << "\n";
    delete conn_;
  } else {
    // Let the IndexReaderConnection execute in its own thread from
    // this point on.
    std::thread t = std::thread(LaunchConnection, conn_);
    t.detach();
  }
  conn_ = nullptr;
  io_service_->post(std::bind(&IndexReaderServer::StartAccept, this));
}

IndexReaderServer::~IndexReaderServer() {
  delete conn_;
  DeleteAllConnections();
}
}
