
#include "cmdline.hpp"
#include "spparray.hpp"
#include "spio.hpp"

namespace sptl {

  void ex() {
    parray<int> xs = { 10, 20, 30, 40, 50 };
    std::cout << "xs\t" << xs << std::endl;
    parray<int> ys;
    std::cout << "ys\t" << ys << std::endl;
    ys.tabulate(xs.size() - 1, [&] (size_t i) {
      return xs[i];
    });
    std::cout << "ys\t" << ys << std::endl;
  }
  
} // end namespace

int main(int argc, char** argv) {
  deepsea::cmdline::set(argc, argv);
  sptl::ex();
  return 0;
}
