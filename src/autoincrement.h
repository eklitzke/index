// -*- C++ -*-
// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#ifndef SRC_AUTOINCREMENT_H_
#define SRC_AUTOINCREMENT_H_

namespace codesearch {

template<typename T>
class AutoIncrement {
 public:
  explicit AutoIncrement() :val_(0) {}
  T inc() { return val_++; }
 private:
  T val_;
};

}  // index

#endif  // SRC_AUTOINCREMENT_H_
