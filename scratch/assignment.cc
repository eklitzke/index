#include <iostream>

class Foo {
 public:
  Foo() { std::cout << "default ctor\n"; }
  Foo(const Foo &other) { std::cout << "copy ctor\n"; }
  Foo& operator=(const Foo &other) {
    std::cout << "assignment \n";
    x_ = other.x_;
    return *this;
  }
 protected:
  int x_;
};

int main(int argc, char *argv[]) {
  Foo foo;
  Foo bar = foo;
  bar = foo;
  return 0;
}
