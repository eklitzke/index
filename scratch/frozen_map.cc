#include <iostream>
#include <vector>
#include <utility>

#include "../src/frozen_map.h"

int main(int argc, char *argv[]) {
  std::vector<std::pair<int, int> > pairs;
  for (int i = 0; i < 100; i++) {
    pairs.emplace_back(i, i + 1);
  }
  FrozenMap<int, int> frozen(pairs);

  for (int i = 0; i <= 10; i++) {
    auto it = frozen.lower_bound(i * 10);
    std::cout << "i = " << (i * 10) << ", is end? " <<
      (it == frozen.end()) << "\n";
    if (it != frozen.end()) {
      std::cout << "it -> (" << it->first << ", " << it->second << ")\n";
    }
  }
  return 0;
}
