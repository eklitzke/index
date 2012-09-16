// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_STRATEGY_H_
#define SRC_STRATEGY_H_

#include <cassert>
#include <ostream>
#include <string>

namespace codesearch {
class SearchStrategy {
 public:
  SearchStrategy() :value_(LEXICOGRAPHIC_SORT) {}
  SearchStrategy(std::uint8_t value) :value_(value) {
    assert(value == 1 || value == 2);
  }

  // This strategy means that we break a search term into its
  // constituent ngrams, sort the list of unique ngrams
  // lexicographically, and then query the index in the sorted
  // order. This can be beneficial because when we do a binary search
  // on the index, we save the lower bound from the previous ngram,
  // and do the binary search with that lower bound. For instance, if
  // we do a search for "void", we'll search first for "oid" and then
  // for "voi". When we do the search for "voi" we'll start our binary
  // search with a lower bound of wherever "oid" was, which means that
  // we need to consider a smaller section of the index.
  static const std::uint8_t LEXICOGRAPHIC_SORT = 1;

  // This strategy means that we break a search term into its
  // constituent ngrams and then sort the list of unique ngrams by
  // frequency in the index. We query the index in order of rarest
  // ngram to most common ngram. The reason that this can be helpful
  // is if the search term contains any rare ngrams, we may get to
  // shortcut early. For instance, if the search is for "char ZZX" the
  // ngram "ZZX" will be very rare but the ngrams like "cha" and "har"
  // will be common. We may be able to short-circuit the ngram lookup
  // immediately after doing the query for "ZZX" and skip lookups on
  // the other, slow ngrams.
  //
  // To use the FREQUENCY_SORT strategy requires building the sorted
  // list of ngrams as a FrozenMap, which is a one-time cost that can
  // take a while; it also uses a bit more memory.
  static const std::uint8_t FREQUENCY_SORT     = 2;

  inline bool operator==(std::uint8_t value) const { return value_ == value; }
  inline std::uint8_t value() const { return value_; }

  inline std::string name() const {
    switch (value_) {
      case LEXICOGRAPHIC_SORT:
        return "LEXICOGRAPHIC_SORT";
        break;
      case FREQUENCY_SORT:
        return "FREQUENCY_SORT";
        break;
      default:
        assert(false);  // not reached
    }
    return "";
  }

 private:
  std::uint8_t value_;
};

// Interpret a strategy string that would be passed in as a command
// line option.
inline SearchStrategy InterpretStrategy(const std::string &name, bool *error) {
  if (name == "lexicographic") {
    *error = false;
    return SearchStrategy(SearchStrategy::LEXICOGRAPHIC_SORT);
  } else if (name == "frequency") {
    *error = false;
    return SearchStrategy(SearchStrategy::FREQUENCY_SORT);
  }
  *error = true;
  return SearchStrategy();
}
}  // namespace codesearch

namespace std {
inline ostream& operator<<(ostream &os, const codesearch::SearchStrategy &s) {
  os << s.name();
  return os;
}
}  // namespace std

#endif  // SRC_STRATEGY_H_
