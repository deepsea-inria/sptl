
#include <math.h>

#include "cmdline.hpp"
#include "spgranularity.hpp"

namespace sptl {

  long long fib_seq(long long n) {
    if (n == 0 || n == 1) {
      return n;
    } else {
      return fib_seq(n - 1) + fib_seq(n - 2);
    }
  }

  double phi = (1 + sqrt(5)) / 2;
  
  long long fib_par(long long n) {
    long long result = 0;
    spguard([&] { return pow(phi, n); }, [&] {
      if (n == 0 || n == 1) {
	result = n;
	return;
      }
      long long a = 0, b = 0;
      fork2([&] { a = fib_par(n - 1); },
	    [&] { b = fib_par(n - 2); });
      result = a + b;
    }, [&] {
      result = fib_seq(n);
    });
    return result;
  }
  
} // end namespace

int main(int argc, char** argv) {
  deepsea::cmdline::set(argc, argv);
  int n = deepsea::cmdline::parse_or_default_int("n", 30);
  long long result = sptl::fib_par(n);
  std::cout << "result\t" << result << std::endl;
  return 0;
}
