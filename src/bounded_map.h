// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_BOUNDED_MAP_H_
#define SRC_BOUNDED_MAP_H_

#include <cassert>
#include <map>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "./util.h"
#include <glog/logging.h>

namespace codesearch {

#define ORDERED_MAP 1

template <typename K, typename V>
class BoundedMap {
 public:
  BoundedMap(std::size_t max_keys, std::size_t max_vals)
      :max_keys_(max_keys), max_vals_(max_vals) {}
  BoundedMap(const BoundedMap &other) = delete;

#ifdef ORDERED_MAP
  typedef std::map<K, std::vector<V> > map_type;
#else
  typedef std::unordered_map<K, std::vector<V> > map_type;
#endif

  // Insert a key/value to the map
  bool insert(const K &key, const V &val) {
    ScopedTimer t("bounded_map insert");
    std::lock_guard<std::mutex> guard(mut_);
    if (map_.size() < max_keys_) {
      if (key > max_key_) {
        max_key_ = key;
      }

#ifdef ORDERED_MAP
      auto pos = map_.lower_bound(key);
#else
      auto pos = map_.find(key);
#endif
      if (pos == map_.end() || pos->first != key) {
        map_.insert(pos, std::make_pair(key, std::vector<V>{val}));
        //LOG(INFO) << "inserted file_id " << key.file_id() << "\n";
      } else if (pos->second.size() < max_vals_) {
        pos->second.push_back(val);
      } else {
        return false;  // the vector was full
      }
      return true;  // we inserted something
    } else if (key <= max_key_){
#ifdef ORDERED_MAP
      auto pos = map_.lower_bound(key);
#else
      auto pos = map_.find(key);
#endif
      if (pos == map_.end() || pos->first != key) {
        assert(key < max_key_);
        map_.insert(pos, std::make_pair(key, std::vector<V> {val}));
#ifdef ORDERED_MAP
        map_.erase(--map_.rbegin().base());
#else
        map_.erase(max_key_);
#endif
        //LOG(INFO) << "inserted file_id " << key.file_id() <<
        // " and removed " << max_key_.file_id() << "\n";
        max_key_ = key;

      } else if (pos->second.size() < max_vals_) {
        pos->second.push_back(val);
      } else {
        return false;  // the vector was full
      }
      return true;  // we inserted something
    }
    return false;
  }

  // Get a copy of the map
  const map_type & map() const {
    std::lock_guard<std::mutex> guard(mut_);
    return map_;
  }

  bool IsFull() const {
    std::lock_guard<std::mutex> guard(mut_);
    return map_.size() >= max_keys_;
  }

 protected:
  mutable std::mutex mut_;
  map_type map_;

 private:
  const std::size_t max_keys_;
  const std::size_t max_vals_;
  K max_key_;
};
}  // namespace codesearch
#endif
