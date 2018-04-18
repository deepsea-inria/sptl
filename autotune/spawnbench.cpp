
#include <chrono>
#include <iostream>

#include "cmdline.hpp"
#include "spreduce.hpp"
#include "spio.hpp"

namespace sptl {

  int* array;

    // This must execute EXACTLY n forks.
  uint64_t spawntree_seq(uint64_t n, uint64_t i) {
    uint64_t r;
    if (n==0) {
      return array[i];
    }
    // First we split without losing any:
    uint64_t half1 = n / 2;
    uint64_t half2 = half1 + (n % 2);
    // We subtract one from our total, because of *this* spawn:
    uint64_t x,y;
    y = spawntree_seq(half1, i);
    x = spawntree_seq(half2 - 1, i + half1);
    r = x + y;
    return r;
  }

  // This must execute EXACTLY n forks.
  uint64_t spawntree(uint64_t n, uint64_t i) {
    uint64_t r;
    spguard([&] { return n; }, [&] {
      if (n==0) {
	r = 1;
	return;
      }
      // First we split without losing any:
      uint64_t half1 = n / 2;
      uint64_t half2 = half1 + (n % 2);
      // We subtract one from our total, because of *this* spawn:
      uint64_t x,y;
      fork2([&] {
	y = spawntree(half1, i);
      }, [&] {
	x = spawntree(half2 - 1, i + half1);
      });
      r = x + y;
    }, [&] {
      r = spawntree_seq(n, i);
    });
    return r;
  }
  
  void bench(uint64_t n) {
    parray<int> xs(n, [] (size_type i) { return i; });
    array = xs.begin();
    auto start = std::chrono::system_clock::now();
    int result = spawntree(n, 0);
    auto end = std::chrono::system_clock::now();
    std::chrono::duration<float> diff = end - start;
    printf ("exectime %.3lf\n", diff.count());
    printf ("result %d\n", result); 
  }
  
} // end namespace

int main(int argc, char** argv) {
  deepsea::cmdline::set(argc, argv);
  uint64_t n = deepsea::cmdline::parse_or_default_int("n", 200000000);
  sptl::bench(n);
  return 0;
}
