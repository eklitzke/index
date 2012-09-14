// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include <boost/asio.hpp>
#include <boost/asio/error.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>

#include <iostream>
#include <fstream>
#include <mutex>
#include <thread>
#include <string>

#include "./config.h"
#include "./index.pb.h"
#include "./util.h"

namespace po = boost::program_options;

class QueryGeneratorExhausted : public std::exception {};

class QueryGenerator {
public:
  QueryGenerator(const std::string &queries, std::size_t limit = 0)
    :generated_(0), limit_(limit) {
    std::ifstream input(queries);
    while (true) {
      std::string line;
      std::getline(input, line);
      if (input.fail() || input.eof()) {
        break;
      }
      if (line.empty() || line[0] == '#') {
        continue;
      }
      for (std::string::size_type i = 1; i <= line.size(); i++) {
        queries_.push_back(line.substr(0, i));
      }
    }
    it_ = queries_.end() - 1;
  }

  const std::string& Next() {
    std::lock_guard<std::mutex> guard(mut_);
    if (generated_++ >= limit_) {
      throw QueryGeneratorExhausted();
    }
    it_++;
    if (it_ == queries_.end()) {
      it_ = queries_.begin();
    }
    return *it_;
  }

private:
  std::vector<std::string> queries_;
  std::vector<std::string>::const_iterator it_;
  std::mutex mut_;
  std::size_t generated_;
  std::size_t limit_;
};


struct TimingData {
  TimingData() :connect_time_ms(0), query_ms(0), query_count(0) {}
  long connect_time_ms;
  long query_ms;
  std::size_t query_count;
};


class QueryThread {
public:
  QueryThread(QueryGenerator *generator,
              bool verbose,
              std::size_t query_limit,
              int port)
    :generator_(generator), verbose_(verbose), query_limit_(query_limit),
     port_(port) {}

  void Run() {
    codesearch::Timer connect_timer;

    boost::asio::io_service io;

    boost::asio::ip::tcp::resolver resolver(io);
    boost::asio::ip::tcp::resolver::query query(
        "127.0.0.1", boost::lexical_cast<std::string>(port_));

    boost::asio::ip::tcp::socket socket(io);
    boost::asio::connect(socket, resolver.resolve(query));
    timing_.connect_time_ms = connect_timer.elapsed_us();

    codesearch::Timer query_timer;
    codesearch::Timer total_timer;
    char response_size[sizeof(std::uint64_t)];
    try {
      codesearch::RPCRequest request;
      codesearch::SearchQueryRequest *search_request =\
        request.mutable_search_query();
      search_request->set_limit(query_limit_);
      while (true) {
        search_request->set_query(generator_->Next());

        std::string serialized_req;
        request.SerializeToString(&serialized_req);

        std::string request_buf = codesearch::Uint64ToString(
            serialized_req.size());
        request_buf += serialized_req;

        query_timer.reset();
        boost::asio::write(socket, boost::asio::buffer(request_buf),
                           boost::asio::transfer_all());

        std::size_t bytes_read = socket.read_some(
            boost::asio::buffer(response_size, sizeof(response_size)));
        assert(bytes_read == sizeof(response_size));

        std::uint64_t response_size_int = codesearch::ToUint64(
            std::string(response_size, 8));

        std::unique_ptr<char[]> full_response(new char[response_size_int]);
        boost::asio::read(socket,
                          boost::asio::buffer(full_response.get(),
                                              response_size_int));
        if (verbose_) {
          std::stringstream ss;
          ss << std::left << std::setw(3) << query_timer.elapsed_ms() <<
            search_request->query() << "\n";
          std::cout << ss.str() << std::flush;
        }
        timing_.query_count++;
      }
    } catch (const boost::system::system_error &error) {
      if (error.code() == boost::asio::error::eof) {
        std::cerr << "\nserver unexpectedly closed connection!" << std::endl;
      }
      throw;
    } catch (const QueryGeneratorExhausted &error) { }

    timing_.query_ms = total_timer.elapsed_ms();
  }

  TimingData timing() const { return timing_; }

private:
  QueryGenerator *generator_;
  bool verbose_;
  std::size_t query_limit_;
  int port_;
  TimingData timing_;
};

int main(int argc, char **argv) {
  // Declare the supported options.
  po::options_description desc("Allowed options");
  desc.add_options()
      ("help,h", "produce help message")
      ("port,p", po::value<int>()->default_value(codesearch::default_rpc_port),
       "the port to connect on")
      ("limit", po::value<std::size_t>()->default_value(10),
       "the number of responses to return")
      ("iterations,i", po::value<std::size_t>()->default_value(1000))
      ("concurrency,c", po::value<std::size_t>()->default_value(1),
       "the number of client threads")
      ("file,f", po::value<std::string>(), "the query file")
      ("verbose,v", "run verbosely")
      ;

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return 0;
  }

  if (!vm.count("file")) {
    std::cout << "error, must pass in a queries file with -f/--file\n";
    return 0;
  }

  std::size_t iterations = vm["iterations"].as<std::size_t>();
  QueryGenerator generator(vm["file"].as<std::string>(), iterations);
  std::vector<std::pair<QueryThread *, std::thread> > threads;
  std::size_t concurrency = vm["concurrency"].as<std::size_t>();

  codesearch::Timer timer;
  for (std::size_t i = 0; i < concurrency; i++) {
    QueryThread *qt = new QueryThread(&generator,
                                      vm.count("verbose"),
                                      vm["limit"].as<std::size_t>(),
                                      vm["port"].as<int>());
    std::thread thr(&QueryThread::Run, qt);
    threads.push_back(std::make_pair(qt, std::move(thr)));
  }

  std::vector<TimingData> timings;
  for (auto &pair : threads) {
    pair.second.join();
    timings.push_back(pair.first->timing());
    delete pair.first;
  }

  long total_ms = timer.elapsed_ms();
  double avg_ms = 0;

  for (const auto &timing : timings) {
    double weight =  static_cast<double>(timing.query_count) /
      static_cast<double>(iterations);

    avg_ms += static_cast<double>(timing.query_ms) /
      static_cast<double>(timing.query_count) *
      weight;
  }
  double qps = static_cast<double>(iterations) / total_ms * 1000;

  std::cout << std::fixed << std::setprecision(1);
  std::cout << iterations << " total searches in " << std::setprecision(3) <<
    (static_cast<double>(total_ms) / 1000.0) << " sec (" << avg_ms <<
    " ms per query / " << std::setprecision(0) << qps << " qps)" << std::endl;

  return 0;
}
