// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_UTIL_H_
#define SRC_UTIL_H_

#include <array>
#include <cassert>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>

#include <endian.h>
#include <glog/logging.h>

#include "./index.pb.h"

namespace codesearch {
std::string ConstructShardPath(const std::string &index_directory,
                               const std::string &name,
                               std::uint32_t shard_num);

inline std::string Uint64ToString(std::uint64_t val) {
  const std::uint64_t be_val = htobe64(val);
  return std::string(
      reinterpret_cast<const char *>(&be_val), sizeof(std::uint64_t));
}

inline std::string Uint32ToString(std::uint32_t val) {
  const std::uint32_t be_val = htobe32(val);
  return std::string(
      reinterpret_cast<const char *>(&be_val), sizeof(std::uint32_t));
}

// N.B. using a C-style array or std::array is preferred, because the
// size of the array is checked at compile-time rather than asserted
// at run-time.
inline std::uint64_t ToUint64(const std::string &str) {
  assert(str.size() == sizeof(std::uint64_t));
  return be64toh(*reinterpret_cast<const std::uint64_t*>(str.data()));
}

template <typename byte_type>
inline std::uint64_t ToUint64(const byte_type str[sizeof(std::uint64_t)]) {
  static_assert(sizeof(byte_type) == 1, "invalid byte_type for ToUint64");
  return be64toh(*reinterpret_cast<const std::uint64_t*>(str));
}

template <typename byte_type>
inline std::uint64_t ToUint64(
    const std::array<byte_type, sizeof(std::uint64_t)> &arr) {
  static_assert(sizeof(byte_type) == 1, "invalid byte_type for ToUint64");
  return be64toh(*reinterpret_cast<const std::uint64_t*>(arr.data()));
}

// memcpy seems to be faster than casting to a std::uint32*
inline std::uint32_t ReadUint32(const char *addr) {
  std::uint32_t val;
  memcpy(&val, addr, sizeof(val));
  return be32toh(val);
}

// memcpy seems to be faster than casting to a std::uint64*
inline std::uint64_t ReadUint64(const char *addr) {
  std::uint64_t val;
  memcpy(&val, addr, sizeof(val));
  return be64toh(val);
}

inline std::uint32_t ReadUint32(std::istream *is) {
  std::uint32_t val;
  char buf[sizeof(val)];
  is->read(buf, sizeof(buf));
  memcpy(&val, buf, sizeof(buf));
  return be32toh(val);
}

inline std::uint64_t ReadUint64(std::istream *is) {
  std::uint64_t val;
  char buf[sizeof(val)];
  is->read(buf, sizeof(buf));
  memcpy(&val, buf, sizeof(buf));
  return be64toh(val);
}

// Get padding to word align something of some size.
inline std::string GetWordPadding(std::size_t size) {
  std::size_t mantissa = size % 8;
  if (mantissa) {
    return std::string(8 - mantissa, '\0');
  }
  return "";
}

// Get the size of a file
std::uint64_t FileSize(std::istream *is);

// Read the header out of a shard, and sanity check it.
SSTableHeader ReadHeader(std::istream *is);


// Initialize glog
inline void InitializeLogging(const char *program_name) {
  google::InitGoogleLogging(program_name);
  LOG(INFO) << " initialized logging\n";
}

// Format a binary string (to be C-escaped).
inline std::string PrintBinaryString(const std::string &str) {
  std::stringstream ss;
  for (const auto &c : str) {
    if (isgraph(c)) {
      ss << static_cast<char>(c);
    } else {
      ss << "\\x";
      ss << std::setfill('0') << std::setw(2) << std::hex <<
        (static_cast<int>(c) & 0xff);
    }
  }
  return ss.str();
}

// Returns true if src is valid UTF-8, false otherwise (and empty
// strings are considered valid).
bool IsValidUtf8(const std::string &src);

// A standard timer class.
class Timer {
 public:

  typedef std::chrono::system_clock clock_type;

  Timer() :start_time_(clock_type::now()) {}
  virtual ~Timer() {};

  void reset() { start_time_ = clock_type::now(); }

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
}  // namespace codesearch
#endif  // SRC_UTIL_H_
