
#include "cmdline.hpp"
#include "sppstring.hpp"
#include "spio.hpp"

namespace sptl {

  void ex() {
    pstring str = "foobar";
    std::cout << "str\t" << str << std::endl;
    pstring str2 = "baz";
    std::cout << "str2\t" << str2 << std::endl;
    str2 += str;
    std::cout << "str2\t" << str2 << std::endl;
  }
  
} // end namespace

int main(int argc, char** argv) {
  deepsea::cmdline::set(argc, argv);
  sptl::ex();
  return 0;
}
