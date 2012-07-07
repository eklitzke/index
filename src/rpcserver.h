// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef _SRC_RPCSERVER_H_
#define _SRC_RPCSERVER_H_

#include <boost/asio.hpp>

#include "./index.h"
#include "./index.pb.h"

namespace codesearch {

class IndexReaderConnection;

class IndexReaderServer {
 public:
  IndexReaderServer(const std::string &db_path,
                    boost::asio::io_service* io_service,
                    const boost::asio::ip::tcp::endpoint &endpoint)
      :db_path_(db_path), io_service_(io_service),
       acceptor_(*io_service, endpoint) {}

  void Start();


 private:
  std::string db_path_;
  boost::asio::io_service *io_service_;
  boost::asio::ip::tcp::acceptor acceptor_;

  void StartAccept();

  void HandleAccept(IndexReaderConnection *conn,
                    const boost::system::error_code& error);
};

}

#endif  // _SRC_RPCSERVER_H_
