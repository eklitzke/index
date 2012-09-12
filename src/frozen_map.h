// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_FROZEN_MAP_H_
#define SRC_FROZEN_MAP_H_

#include <algorithm>
#include <utility>
#include <vector>

#include <boost/iterator/iterator_facade.hpp>

namespace codesearch {
template <typename K, typename V>
class FrozenMapBuilder {
public:
  FrozenMapBuilder() {}

  void insert(const K &key, const V &val) {
    vec_.push_back(std::make_pair(key, val));
  }

  const std::vector<std::pair<K, V> >& vector () {
    std::sort(vec_.begin(), vec_.end());
    return vec_;
  }

private:
  std::vector<std::pair<K, V> > vec_;
};

template <typename K, typename V>
class FrozenMap {

public:
  typedef typename std::vector<std::pair<K, V> >::const_iterator iterator;

 private:
  typedef typename std::vector<std::pair<K, V> > vector_type;

  class key_iterator :
    public boost::iterator_facade<key_iterator,
                                  const K,
                                  std::random_access_iterator_tag,
                                  const K&> {
  public:
    explicit key_iterator(const vector_type *vec, std::ptrdiff_t offset)
      :vector_(vec), offset_(offset) {}

    inline iterator iter() const { return vector_->cbegin() + offset_; }

  private:
    friend class boost::iterator_core_access;

    inline const K& dereference() const { return (*vector_)[offset_].first; }
    inline void increment() { offset_++; }
    inline void decrement() { offset_--; }
    inline void advance(std::ptrdiff_t n) { offset_ += n; }

    inline std::ptrdiff_t distance_to(const key_iterator &other) const {
      return other.offset_ - offset_;
    }

    inline bool equal(const key_iterator &other) const {
#if 0
      return vector_ == other.vector_ && offset_ == other.offset_;
#else
      return offset_ == other.offset_;
#endif
    }

    const vector_type *vector_;
    std::ptrdiff_t offset_;
  };

public:
  explicit FrozenMap() {}
  explicit FrozenMap(const vector_type &vec) :vec_(vec) {}
  explicit FrozenMap(FrozenMapBuilder<K, V> &builder) :vec_(builder.vector()) {}

  FrozenMap& operator=(FrozenMapBuilder<K, V> &builder) {
    vec_ = builder.vector();
    return *this;
  }

private:
  inline key_iterator key_begin() const {
    return key_iterator(&vec_, 0);
  }

  inline key_iterator key_end() const {
    return key_iterator(&vec_, vec_.size());
  }

  inline key_iterator key_lower_bound(const K &key) const {
    return std::lower_bound(key_begin(), key_end(), key);
  }

public:
  inline std::size_t size() const { return vec_.size(); }
  inline bool empty() const { return vec_.empty(); }
  inline iterator begin() const { return vec_.cbegin(); }
  inline iterator end() const { return vec_.cend(); }

  inline iterator lower_bound(const K &key) const {
    return key_lower_bound(key).iter();
  }

private:
  vector_type vec_;
};
}  // namespace codesearch
#endif  // SRC_FROZEN_MAP_H_
