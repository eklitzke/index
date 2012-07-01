#include "./util.h"

#include <boost/filesystem.hpp>

namespace cs {
namespace index {
std::string JoinPath(const std::string &lhs, const std::string &rhs) {
  boost::filesystem::path p(lhs + "/" + rhs);
  //return boost::filesystem::canonical(p).string();
  return p.string();
}
}
}
