// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./sstable_writer.h"
#include "./util.h"
#ifdef USE_SNAPPY
#include <snappy.h>
#endif
#include <cstring>

namespace {
}

namespace codesearch {
void SSTableWriter::Initialize() {
  assert(state_ == UNINITIALIZED);
  idx_out_.open(name_ + ".idx",std::ofstream::binary | std::ofstream::trunc |
                std::ofstream::out);
  data_out_.open(name_ + ".data", std::ofstream::binary |
                 std::ofstream::trunc | std::ofstream::out);
  state_ = INITIALIZED;
}

void SSTableWriter::Add(const std::string &key, const std::string &val) {
  assert(state_ == INITIALIZED);
  std::string padded_key = std::string(8 - key.size(), '\0') + key;

  assert(memcmp(last_key_.c_str(), padded_key.c_str(), 8) <= 0);
  last_key_ = padded_key;
  idx_out_.write(padded_key.c_str(), 8);

  std::string offset_str;
  Uint64ToString(offset_, &offset_str);
  idx_out_.write(offset_str.c_str(), 8);

#ifdef USE_SNAPPY
  std::string compress_data;
  snappy::Compress(val.c_str(), val.size(), &compress_data);
#else
  const std::string &compress_data = val;
#endif

  std::string data_size;
  Uint64ToString(compress_data.size(), &data_size);
  data_out_.write(data_size.c_str(), 8);
  data_out_.write(compress_data.c_str(), compress_data.size());
  offset_ += compress_data.size() + 8;


}

void SSTableWriter::Add(const std::uint64_t key,
                        const google::protobuf::Message &val) {
  std::string key_string;
  Uint64ToString(key, &key_string);
  std::string value_string;
  val.SerializeToString(&value_string);
  Add(key_string, value_string);
}

void SSTableWriter::Add(const std::string &key,
                        const google::protobuf::Message &val) {
  std::string value_string;
  val.SerializeToString(&value_string);
  Add(key, value_string);
}

void SSTableWriter::Add(const google::protobuf::Message &key,
                        const google::protobuf::Message &val) {
  std::string key_string, value_string;
  key.SerializeToString(&key_string);
  val.SerializeToString(&value_string);
  Add(key_string, value_string);
}

void SSTableWriter::Merge() {
  assert(state_ == INITIALIZED);
  state_ = MERGED;
  idx_out_.close();
  data_out_.close();

  std::ifstream idx(name_ + ".idx", std::ifstream::binary | std::ifstream::in);
  std::ifstream data(name_ + ".data",
                     std::ifstream::binary | std::ifstream::in);
  std::ofstream out(name_ + ".sst", std::ofstream::binary |
                    std::ofstream::trunc | std::ofstream::out);

  WriteMergeSize(&idx, &out);
  WriteMergeSize(&data, &out);
  WriteMergeContents(&idx, &out);
  WriteMergeContents(&data, &out);
  assert(boost::filesystem::remove(name_ + ".idx"));
  assert(boost::filesystem::remove(name_ + ".data"));
}

void SSTableWriter::WriteMergeSize(std::ifstream *is, std::ofstream *os) {
  is->seekg(0, std::ifstream::end);
  std::uint64_t file_size = static_cast<std::uint64_t>(is->tellg());
  is->seekg(0, std::ifstream::beg);
  std::string fs;
  Uint64ToString(file_size, &fs);
  os->write(fs.c_str(), 8);
}

void SSTableWriter::WriteMergeContents(std::ifstream *is, std::ofstream *os) {
  const std::streamsize buf_size = 1<<20;
  std::unique_ptr<char[]> buf(new char[static_cast<std::size_t>(buf_size)]);
  while (true) {
    is->read(buf.get(), buf_size);
    os->write(buf.get(), is->gcount());
    if (is->eof()) {
      break;
    }
    assert(!is->fail());
  }
}
}  // namespace codesearch
