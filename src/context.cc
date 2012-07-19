// Copyright 2012, Evan Klitzke <evan@eklitzke.org>

#include "./context.h"

#include <google/protobuf/stubs/common.h>
#include "./mmap.h"

namespace codesearch {
Context::~Context() {
  UnmapFiles();
  google::protobuf::ShutdownProtobufLibrary();
}
}
