// Test to see which is called, an implicit constructor or a
// specialized operator==.

#include <cstring>
#include <iostream>

class String {
 public:
  String(const char *c_str = "") :c_str_(c_str) {
    std::cout << "ctor of String(" << c_str << ")\n";
  }

  bool operator==(const String& other) {
    std::cout << "in String overload\n";
    return strcmp(c_str_, other.c_str()) == 0;
  }

  bool operator==(const char *other) {
    std::cout << "in const char * overload\n";
    return strcmp(c_str_, other) == 0;
  }

  const char* c_str() const { return c_str_; }

 private:
  const char *c_str_;
};

int main(int argc, char *argv[]) {
  String s("hello");
  s == "world";
  return 0;
}
