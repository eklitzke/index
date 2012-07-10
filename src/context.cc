#include "./context.h"

#include "./mmap.h"

namespace codesearch {
Context::~Context() {
  UnmapFiles();
}
}
