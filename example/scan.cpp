
#include "cmdline.hpp"
#include "spreduce.hpp"
#include "spio.hpp"

namespace sptl {

  void ex() {
    parray<int> xs = { 10, 20, 30, 40, 50 };
    std::cout << "xs\t" << xs << std::endl;
    parray<int> ys = scan(xs.begin(), xs.end(), 0, [&] (int x, int y) {
		       return x + y;
		     }, forward_exclusive_scan);
    std::cout << "ys\t" << ys << std::endl;
    
    dps::scan(xs.begin(), xs.end(), 0, [&] (int x, int y) {
		       return x + y;
           }, ys.begin(), forward_exclusive_scan); 
    
    std::cout << "ys\t" << ys << std::endl;
  }
  
} // end namespace

int main(int argc, char** argv) {
  deepsea::cmdline::set(argc, argv);
  sptl::ex();
  return 0;
}
