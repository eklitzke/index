// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./sstable_writer.h"
#include "./util.h"
#include "./index.pb.h"

#ifdef USE_SNAPPY
#include <snappy.h>
#endif
#include <cstring>

namespace codesearch {
SSTableWriter::SSTableWriter(const std::string &name,
                             std::size_t key_size,
                             bool use_snappy)
    :name_(name), use_snappy_(use_snappy), state_(WriterState::UNINITIALIZED),
     index_size_(0), data_size_(0), num_keys_(0) {
  std::size_t sizediff = key_size % sizeof(std::size_t);
  if (sizediff != 0) {
    key_size += sizeof(std::size_t) - sizediff;
  }
  key_size_ = key_size;
  last_key_ = std::string(key_size, '\0');

  idx_out_.open(name_ + ".idx",std::ofstream::binary | std::ofstream::trunc |
                std::ofstream::out);
  data_out_.open(name_ + ".data", std::ofstream::binary |
                 std::ofstream::trunc | std::ofstream::out);
  state_ = WriterState::INITIALIZED;
}

void SSTableWriter::Add(const std::string &key, const std::string &val) {
  assert(state_ == WriterState::INITIALIZED);
  assert(key.size() <= key_size_);
  num_keys_++;

  std::string padded_key = std::string(key_size_ - key.size(), '\0') + key;

  if (index_size_ == 0) {
    min_key_ = padded_key;
  }

  // ensure the keys are inserted in sorted order
  assert(memcmp(last_key_.c_str(), padded_key.c_str(), key_size_) <= 0);
  last_key_ = padded_key;

  idx_out_.write(padded_key.c_str(), key_size_);
  assert(!idx_out_.fail() && !idx_out_.eof());

  std::string offset_str = Uint64ToString(data_size_);
  idx_out_.write(offset_str.c_str(), sizeof(std::uint64_t));
  assert(!idx_out_.fail() && !idx_out_.eof());
  index_size_ += key_size_ + sizeof(std::uint64_t);

  std::string compress_data;
  if (use_snappy_) {
    snappy::Compress(val.c_str(), val.size(), &compress_data);
  } else {
    compress_data = val;
  }

  std::string data_size = Uint64ToString(compress_data.size());
  data_out_.write(data_size.c_str(), sizeof(std::uint64_t));
  assert(!data_out_.fail() && !data_out_.eof());
  data_out_.write(compress_data.c_str(), compress_data.size());
  assert(!data_out_.fail() && !data_out_.eof());
  std::string padding = GetWordPadding(compress_data.size());
  if (padding.empty()) {
    data_size_ += sizeof(std::uint64_t) + compress_data.size();
  } else {
    data_out_.write(padding.c_str(), padding.size());
    assert(!data_out_.fail() && !data_out_.eof());
    data_size_ += sizeof(std::uint64_t) + compress_data.size() + padding.size();
  }
}

void SSTableWriter::Add(const std::uint64_t key,
                        const google::protobuf::Message &val) {
  std::string key_string = Uint64ToString(key);
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
  assert(state_ == WriterState::INITIALIZED);
  state_ = WriterState::MERGED;
  idx_out_.close();
  data_out_.close();

  std::ifstream idx(name_ + ".idx", std::ifstream::binary | std::ifstream::in);
  std::ifstream data(name_ + ".data",
                     std::ifstream::binary | std::ifstream::in);
  std::ofstream out(name_ + ".sst", std::ofstream::binary |
                    std::ofstream::trunc | std::ofstream::out);

  assert(index_size_ == FileSize(&idx, &out));
  assert(data_size_ == FileSize(&data, &out));
  assert(min_key_.size() == key_size_);
  assert(last_key_.size() == key_size_);

  SSTableHeader header;
  header.set_index_size(index_size_);
  header.set_data_size(data_size_);
  header.set_min_value(min_key_);
  header.set_max_value(last_key_);
  header.set_key_size(key_size_);
  header.set_num_keys(num_keys_);
  header.set_index_offset(0);
  header.set_data_offset(0);
  header.set_uses_snappy(use_snappy_);

  std::uint64_t header_size = header.ByteSize();
  std::string header_size_str = Uint64ToString(header_size);

  out.write(header_size_str.c_str(), header_size_str.size());
  header.SerializeToOstream(&out);
  std::streampos header_end_offset = out.tellp();
  assert(header_end_offset != -1);
  std::string padding = GetWordPadding(header_size);
  out.write(padding.c_str(), padding.size());

  std::uint64_t index_offset = WriteMergeContents(&idx, &out);
  std::uint64_t data_offset = WriteMergeContents(&data, &out);
  assert(boost::filesystem::remove(name_ + ".idx"));
  assert(boost::filesystem::remove(name_ + ".data"));

  header.set_index_offset(index_offset);
  header.set_data_offset(data_offset);
  std::uint64_t new_header_size = header.ByteSize();
  assert(header_size == new_header_size);
  out.seekp(header_size_str.size(), std::ostream::beg);
  header.SerializeToOstream(&out);
  assert(out.tellp() == header_end_offset);
}

std::uint64_t SSTableWriter::FileSize(std::ifstream *is, std::ofstream *os) {
  is->seekg(0, std::ifstream::end);
  std::uint64_t file_size = static_cast<std::uint64_t>(is->tellg());
  is->seekg(0, std::ifstream::beg);
  return file_size;
}

std::uint64_t SSTableWriter::WriteMergeContents(std::ifstream *is,
                                                std::ofstream *os) {
  std::streampos position = os->tellp();
  assert(position != -1);
  std::uint64_t offset = static_cast<std::uint64_t>(position);

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
  return offset;
}
}  // namespace codesearch
