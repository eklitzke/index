// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_NGRAM_INDEX_WRTITER_H_
#define SRC_NGRAM_INDEX_WRTITER_H_

#include "./autoincrement_index_writer.h"
#include "./index_writer.h"

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

namespace codesearch {
class NGramIndexWriter {
 public:
  NGramIndexWriter(const std::string &index_directory,
                   std::size_t ngram_size = 3,
                   std::size_t shard_size = 16 << 20,
                   std::size_t max_threads = 1);

  // Add a file to the index; returns the file id.
  std::uint64_t AddFile(const std::string &canonical_name,
                        const std::string &dir_name,
                        const std::string &file_name);

  std::size_t ngram_size() const { return ngram_size_; }

  ~NGramIndexWriter();

 private:
  AutoIncrementIndexWriter files_index_;

  const std::size_t ngram_size_;

  const std::size_t max_threads_;
  std::size_t threads_running_;
  std::condition_variable cond_;
  std::mutex threads_running_mut_;
  std::mutex ngrams_mut_;

  void AddFileThread(std::uint64_t file_id,
                     const std::string &canonical_name);


  // Used to notify the condition variable
  void Notify();

  // Wait for all threads to finish
  void Wait();

  struct PositionsContainer {
    PositionsContainer(
        std::vector<PositionValue> positions_,
        std::map<std::string, std::vector<std::uint64_t> > positions_map_)
        :positions(positions_), positions_map(positions_map_) {}

    PositionsContainer(PositionsContainer &&other)
        :positions(std::move(other.positions)),
         positions_map(std::move(other.positions_map)) {}

    PositionsContainer(const PositionsContainer &other) = delete;
    PositionsContainer& operator=(const PositionsContainer &other) = delete;

    std::vector<PositionValue> positions;
    std::map<std::string, std::vector<std::uint64_t> > positions_map;
  };

  class FileRotateThread {
   public:
    FileRotateThread(const std::string &index_directory,
                     std::size_t shard_size);

    void AddLines(std::size_t file_id,
                  PositionsContainer positions);

    void Start() {
      // acquire the run lock
      assert(run_mutex_.try_lock() == true);

      // blocks until Stop() is called
      run_mutex_.lock();
      run_mutex_.unlock();
    }
    void Stop() {
      run_mutex_.unlock();
    }

    // Force the indexes to be synced to disk when the thread finishes
    ~FileRotateThread() { MaybeRotate(true); }

   private:
    // The index writer for the "ngrams" index
    IndexWriter index_writer_;

    // The index writer for the "lines" index
    AutoIncrementIndexWriter lines_index_;

    // A map of file_id -> positions
    std::map<std::uint64_t, PositionsContainer> positions_map_;

    // The last file synced to disk
    std::uint64_t last_written_file_;

    // Lock held during the AddLines method
    std::mutex add_lines_mutex_;

    // Lock help by Start()/Stop()
    std::mutex run_mutex_;

    // Estimate the size of the index that will be written
    std::size_t EstimateSize(std::size_t num_keys, std::size_t num_vals);

    // Rotate the index writer, if the index is large enough
    void MaybeRotate(bool force = false);
  };

  FileRotateThread file_rotate_;
  std::thread file_rotate_thread_;
};
}

#endif
