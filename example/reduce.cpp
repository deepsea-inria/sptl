
#include "cmdline.hpp"
#include "spreduce.hpp"
#include "spio.hpp"

namespace sptl {

  void ex() {
    parray<int> xs = { 10, 20, 30, 40, 50 };
    std::cout << "xs\t" << xs << std::endl;
    int x = reduce(xs.begin(), xs.end(), 0, [&] (int x, int y) {
	      return x + y;
	    });
    std::cout << "x\t" << x << std::endl;
  }
  
} // end namespace

int main(int argc, char** argv) {
  sptl::launch(argc, argv, [&] {
    sptl::ex();
  });
  return 0;
}
