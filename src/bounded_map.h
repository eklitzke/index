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

enum class BoundedMapInsertionResult : std::uint8_t {
  INSERT_SUCCESSFUL = 0,
  KEY_TOO_LARGE     = 1,
  VAL_LIST_TOO_LONG = 2
};

#define USE_ORDERED_MAP 1

template <typename K, typename V>
class BoundedMap {
 public:
  BoundedMap(std::size_t max_keys, std::size_t max_vals)
      :max_keys_(max_keys), max_vals_(max_vals) {}
  BoundedMap(const BoundedMap &other) = delete;
  BoundedMap& operator=(const BoundedMap &other) = delete;

#ifdef USE_ORDERED_MAP
  typedef std::map<K, std::vector<V> > map_type;
#else
  typedef std::unordered_map<K, std::vector<V> > map_type;
#endif

  // Insert a key/value to the map
  BoundedMapInsertionResult insert(const K &key, const V &val) {
    std::lock_guard<std::mutex> guard(mut_);
    if (map_.size() < max_keys_) {
      if (key > max_key_) {
        max_key_ = key;
      }

#ifdef USE_ORDERED_MAP
      auto pos = map_.lower_bound(key);
#else
      auto pos = map_.find(key);
#endif
      if (pos == map_.end() || pos->first != key) {
        map_.insert(pos, std::make_pair(key, std::vector<V>{val}));
      } else if (pos->second.size() < max_vals_) {
        pos->second.push_back(val);
      } else {
        return BoundedMapInsertionResult::VAL_LIST_TOO_LONG;
      }
      return BoundedMapInsertionResult::INSERT_SUCCESSFUL;
    } else if (key <= max_key_){
#ifdef USE_ORDERED_MAP
      auto pos = map_.lower_bound(key);
#else
      auto pos = map_.find(key);
#endif
      if (pos == map_.end() || pos->first != key) {
        assert(key < max_key_);
        map_.insert(pos, std::make_pair(key, std::vector<V> {val}));
#ifdef USE_ORDERED_MAP
        map_.erase(--map_.rbegin().base());
#else
        map_.erase(max_key_);
#endif
        max_key_ = (--map_.rbegin().base())->first;

      } else if (pos->second.size() < max_vals_) {
        pos->second.push_back(val);
      } else {
        return BoundedMapInsertionResult::VAL_LIST_TOO_LONG;
      }
      return BoundedMapInsertionResult::INSERT_SUCCESSFUL;
    }
    return BoundedMapInsertionResult::KEY_TOO_LARGE;
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
