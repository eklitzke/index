// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_UTIL_H_
#define SRC_UTIL_H_

#include <array>
#include <string>
#include <cassert>
#include <chrono>

#include <endian.h>
#include <glog/logging.h>

namespace codesearch {
std::string ConstructShardPath(const std::string &index_directory,
                               const std::string &name,
                               std::uint32_t shard_num);

inline std::string Uint64ToString(std::uint64_t val) {
  const std::uint64_t be_val = htobe64(val);
  return std::string(reinterpret_cast<const char *>(&be_val), 8);
}

inline std::uint64_t ToUint64(const std::string &str) {
  assert(str.size() == sizeof(std::uint64_t));
  return be64toh(*reinterpret_cast<const std::uint64_t*>(str.data()));
}

template<typename int_type>
std::uint64_t ToUint64(const std::array<int_type,
                       sizeof(std::uint64_t)> &data) {
  return (
    (static_cast<std::uint64_t>(std::get<0>(data)) << 54) +
    (static_cast<std::uint64_t>(std::get<1>(data)) << 48) +
    (static_cast<std::uint64_t>(std::get<2>(data)) << 40) +
    (static_cast<std::uint64_t>(std::get<3>(data)) << 32) +
    (static_cast<std::uint64_t>(std::get<4>(data)) << 24) +
    (static_cast<std::uint64_t>(std::get<5>(data)) << 16) +
    (static_cast<std::uint64_t>(std::get<6>(data)) << 8) +
    (static_cast<std::uint64_t>(std::get<7>(data))));
}

// Get padding to word align something of some size.
inline std::string GetWordPadding(std::size_t size) {
  std::size_t mantissa = size % 8;
  if (mantissa) {
    return std::string(8 - mantissa, '\0');
  }
  return "";
}

// Format a binary string (to be C-escaped).
std::string PrintBinaryString(const std::string &str);

// Returns true if src is valid UTF-8, false otherwise (and empty
// strings are considered valid).
bool IsValidUtf8(const std::string &src);

// A standard timer class.
class Timer {
 public:

  typedef std::chrono::system_clock clock_type;

  Timer() :start_time_(clock_type::now()) {}

  long elapsed_s() { return elapsed_count<std::chrono::seconds>(); }
  long elapsed_ms() { return elapsed_count<std::chrono::milliseconds>(); }
  long elapsed_us() { return elapsed_count<std::chrono::microseconds>(); }
  long elapsed_ns() { return elapsed_count<std::chrono::nanoseconds>(); }
 private:
  std::chrono::time_point<std::chrono::system_clock> start_time_;

  template <typename T>
  inline int elapsed_count() {
    return std::chrono::duration_cast<T>(
        clock_type::now() - start_time_).count();
  }
};

class HighResolutionTimer : public Timer {
 public:
  typedef std::chrono::high_resolution_clock clock_type;
};

class ScopedTimer : public Timer {
 public:
  ScopedTimer(const std::string &msg) :msg_(msg) {}
  ~ScopedTimer() {
    //LOG(INFO) << msg_ << ", elapsed = " << elapsed_us() << " us\n";
  }
 private:
  const std::string msg_;
};

}
#endif
